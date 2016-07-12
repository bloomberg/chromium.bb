// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.document;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.Notification;
import android.app.SearchManager;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.StrictMode;
import android.provider.Browser;
import android.support.customtabs.CustomTabsIntent;
import android.text.TextUtils;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.CommandLineInitUtil;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.TraceEvent;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.InstantAppsHandler;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.IntentHandler.TabOpenType;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.WarmupManager;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.SeparateTaskCustomTabActivity;
import org.chromium.chrome.browser.firstrun.FirstRunFlowSequencer;
import org.chromium.chrome.browser.metrics.LaunchMetrics;
import org.chromium.chrome.browser.metrics.MediaNotificationUma;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.notifications.NotificationPlatformBridge;
import org.chromium.chrome.browser.partnercustomizations.PartnerBrowserCustomizations;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.DocumentModeAssassin;
import org.chromium.chrome.browser.upgrade.UpgradeActivity;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.chrome.browser.util.UrlUtilities;
import org.chromium.chrome.browser.webapps.ActivityAssigner;
import org.chromium.chrome.browser.webapps.WebappLauncherActivity;

import java.lang.ref.WeakReference;
import java.net.URI;
import java.util.UUID;

/**
 * Dispatches incoming intents to the appropriate activity based on the current configuration and
 * Intent fired.
 */
public class ChromeLauncherActivity extends Activity
        implements IntentHandler.IntentHandlerDelegate {
    /**
     * Extra indicating launch mode used.
     */
    public static final String EXTRA_LAUNCH_MODE =
            "com.google.android.apps.chrome.EXTRA_LAUNCH_MODE";

    /**
     * Whether or not the toolbar should indicate that a tab was spawned by another Activity.
     */
    public static final String EXTRA_IS_ALLOWED_TO_RETURN_TO_PARENT =
            "org.chromium.chrome.browser.document.IS_ALLOWED_TO_RETURN_TO_PARENT";

    private static final String TAG = "document_CLActivity";

    private static final int FIRST_RUN_EXPERIENCE_REQUEST_CODE = 101;

    /**
     * Timeout in ms for reading PartnerBrowserCustomizations provider. We do not trust third party
     * provider by default.
     */
    private static final int PARTNER_BROWSER_CUSTOMIZATIONS_TIMEOUT_MS = 10000;

    /**
     * Maximum delay for initial document activity launch.
     */
    private static final int INITIAL_DOCUMENT_ACTIVITY_LAUNCH_TIMEOUT_MS = 500;

    private static final LaunchMetrics.SparseHistogramSample sIntentFlagsHistogram =
            new LaunchMetrics.SparseHistogramSample("Launch.IntentFlags");

    private IntentHandler mIntentHandler;
    private boolean mIsInLegacyMultiInstanceMode;

    private boolean mIsCustomTabIntent;
    private boolean mIsHerbIntent;

    /** When started with an intent, maybe pre-resolve the domain. */
    private void maybePrefetchDnsInBackground() {
        if (getIntent() != null && Intent.ACTION_VIEW.equals(getIntent().getAction())) {
            String maybeUrl = IntentHandler.getUrlFromIntent(getIntent());
            if (maybeUrl != null) {
                WarmupManager.getInstance().maybePrefetchDnsForUrlInBackground(this, maybeUrl);
            }
        }
    }

    /**
     * Figure out how to route the Intent.  Because this is on the critical path to startup, please
     * avoid making the pathway any more complicated than it already is.  Make sure that anything
     * you add _absolutely has_ to be here.
     */
    @Override
    public void onCreate(Bundle savedInstanceState) {
        // Third-party code adds disk access to Activity.onCreate. http://crbug.com/619824
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
        try {
            super.onCreate(savedInstanceState);
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }
        // This Activity is only transient. It launches another activity and
        // terminates itself. However, some of the work is performed outside of
        // {@link Activity#onCreate()}. To capture this, the TraceEvent starts
        // in onCreate(), and ends in onPause().
        TraceEvent.begin("ChromeLauncherActivity");
        // Needs to be called as early as possible, to accurately capture the
        // time at which the intent was received.
        IntentHandler.addTimestampToIntent(getIntent());
        // Initialize the command line in case we've disabled document mode from there.
        CommandLineInitUtil.initCommandLine(this, ChromeApplication.COMMAND_LINE_FILE);

        // Read partner browser customizations information asynchronously.
        // We want to initialize early because when there is no tabs to restore, we should possibly
        // show homepage, which might require reading PartnerBrowserCustomizations provider.
        PartnerBrowserCustomizations.initializeAsync(getApplicationContext(),
                PARTNER_BROWSER_CUSTOMIZATIONS_TIMEOUT_MS);
        recordIntentMetrics();

        mIsInLegacyMultiInstanceMode =
                MultiWindowUtils.getInstance().shouldRunInLegacyMultiInstanceMode(this);
        mIntentHandler = new IntentHandler(this, getPackageName());
        mIsCustomTabIntent = isCustomTabIntent(getIntent());
        if (!mIsCustomTabIntent) {
            mIsHerbIntent = isHerbIntent();
            mIsCustomTabIntent = mIsHerbIntent;
        }

        Intent intent = getIntent();
        int tabId = IntentUtils.safeGetIntExtra(intent,
                TabOpenType.BRING_TAB_TO_FRONT.name(), Tab.INVALID_TAB_ID);
        boolean incognito = intent.getBooleanExtra(
                IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, false);

        // Check if a web search Intent is being handled.
        String url = IntentHandler.getUrlFromIntent(intent);
        if (url == null && tabId == Tab.INVALID_TAB_ID
                && !incognito && mIntentHandler.handleWebSearchIntent(intent)) {
            finish();
            return;
        }

        // Check if a LIVE WebappActivity has to be brought back to the foreground.  We can't
        // check for a dead WebappActivity because we don't have that information without a global
        // TabManager.  If that ever lands, code to bring back any Tab could be consolidated
        // here instead of being spread between ChromeTabbedActivity and ChromeLauncherActivity.
        // https://crbug.com/443772, https://crbug.com/522918
        if (WebappLauncherActivity.bringWebappToFront(tabId)) {
            ApiCompatibilityUtils.finishAndRemoveTask(this);
            return;
        }

        // The notification settings cog on the flipped side of Notifications and in the Android
        // Settings "App Notifications" view will open us with a specific category.
        if (intent.hasCategory(Notification.INTENT_CATEGORY_NOTIFICATION_PREFERENCES)) {
            NotificationPlatformBridge.launchNotificationPreferences(this, getIntent());
            finish();
            return;
        }

        // Check if we should launch an Instant App to handle the intent.
        ChromeApplication application = (ChromeApplication) getApplication();
        if (InstantAppsHandler.getInstance(application).handleIncomingIntent(
                this, intent, mIsCustomTabIntent)) {
            finish();
            return;
        }

        // Check if we should launch the ChromeTabbedActivity.
        if (!mIsCustomTabIntent && !FeatureUtilities.isDocumentMode(this)) {
            // Launch the First Run Experience for VIEW Intents with URLs before launching
            // ChromeTabbedActivity if necessary.
            if (getIntent() != null && getIntent().getAction() == Intent.ACTION_VIEW
                    && IntentHandler.getUrlFromIntent(getIntent()) != null) {
                if (launchFirstRunExperience()) {
                    return;
                }
            }
            launchTabbedMode();
            finish();
            return;
        }

        // Check if we should launch the FirstRunActivity.  This occurs after the check to launch
        // ChromeTabbedActivity because ChromeTabbedActivity handles FRE in its own way.
        if (launchFirstRunExperience()) {
            return;
        }

        // Check if we should launch a Custom Tab.
        if (mIsCustomTabIntent) {
            launchCustomTabActivity();
            finish();
            return;
        }

        // Force a user to migrate to document mode, if necessary.
        if (DocumentModeAssassin.getInstance().isMigrationNecessary()) {
            Log.d(TAG, "Diverting to UpgradeActivity via ChromeLauncherActivity.");
            UpgradeActivity.launchInstance(this, intent);
            ApiCompatibilityUtils.finishAndRemoveTask(this);
            return;
        }

        // All possible bounces to other activities should have already been enumerated above.
        Log.e(TAG, "User wasn't sent to another Activity.");
        assert false;
        ApiCompatibilityUtils.finishAndRemoveTask(this);
    }

    @Override
    public void onPause() {
        super.onPause();
        TraceEvent.end("ChromeLauncherActivity");
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == FIRST_RUN_EXPERIENCE_REQUEST_CODE) {
            if (resultCode == Activity.RESULT_OK) {
                // User might have opted out during FRE, so check again.
                if (mIsCustomTabIntent) {
                    launchCustomTabActivity();
                    finish();
                } else {
                    launchTabbedMode();
                    finish();
                }
                return;
            }

            // TODO(aruslan): FAIL.
            ApiCompatibilityUtils.finishAndRemoveTask(this);
        }
    }

    @Override
    public void processWebSearchIntent(String query) {
        Intent searchIntent = new Intent(Intent.ACTION_WEB_SEARCH);
        searchIntent.putExtra(SearchManager.QUERY, query);
        startActivity(searchIntent);
    }

    @Override
    public void processUrlViewIntent(String url, String referer, String headers,
            IntentHandler.TabOpenType tabOpenType, String externalAppId,
            int tabIdToBringToFront, boolean hasUserGesture, Intent intent) {
        assert false;
    }

    /**
     * @return Whether or not an Herb prototype may hijack an Intent.
     */
    public static boolean canBeHijackedByHerb(Intent intent) {
        String url = IntentHandler.getUrlFromIntent(intent);

        // Only VIEW Intents with URLs are rerouted to Custom Tabs.
        if (intent == null || !TextUtils.equals(Intent.ACTION_VIEW, intent.getAction())
                || TextUtils.isEmpty(url)) {
            return false;
        }

        // Don't reroute Chrome Intents.
        Context context = ContextUtils.getApplicationContext();
        if (TextUtils.equals(context.getPackageName(),
                IntentUtils.safeGetStringExtra(intent, Browser.EXTRA_APPLICATION_ID))) {
            return false;
        }

        // Don't reroute internal chrome URLs.
        try {
            URI uri = URI.create(url);
            if (UrlUtilities.isInternalScheme(uri)) return false;
        } catch (IllegalArgumentException e) {
            return false;
        }

        return true;
    }

    /**
     * @return Whether or not a Custom Tab will be forcefully used for the incoming Intent.
     */
    private boolean isHerbIntent() {
        if (!canBeHijackedByHerb(getIntent())) return false;

        // Different Herb flavors handle incoming intents differently.
        String flavor = FeatureUtilities.getHerbFlavor();
        if (TextUtils.isEmpty(flavor)
                || TextUtils.equals(ChromeSwitches.HERB_FLAVOR_DISABLED, flavor)) {
            return false;
        } else if (TextUtils.equals(flavor, ChromeSwitches.HERB_FLAVOR_ELDERBERRY)) {
            return IntentUtils.safeGetBooleanExtra(getIntent(),
                    ChromeLauncherActivity.EXTRA_IS_ALLOWED_TO_RETURN_TO_PARENT, true);
        } else if (TextUtils.equals(flavor, ChromeSwitches.HERB_FLAVOR_ANISE)
                || TextUtils.equals(flavor, ChromeSwitches.HERB_FLAVOR_BASIL)
                || TextUtils.equals(flavor, ChromeSwitches.HERB_FLAVOR_DILL)) {
            // Only Intents without NEW_TASK and NEW_DOCUMENT will trigger a Custom Tab.
            boolean isSameTask = (getIntent().getFlags() & Intent.FLAG_ACTIVITY_NEW_TASK) == 0;
            boolean isSameDocument =
                    (getIntent().getFlags() & Intent.FLAG_ACTIVITY_NEW_DOCUMENT) == 0;
            Log.d(TAG, "Herb Intent proprties -- SAME TASK: "
                    + isSameTask + ", SAME DOCUMENT: " + isSameDocument);
            return isSameTask && isSameDocument;
        } else if (TextUtils.equals(flavor, ChromeSwitches.HERB_FLAVOR_CHIVE)) {
            // Send all View Intents to the main browser.
            return false;
        } else {
            assert false;
            return false;
        }
    }

    /**
     * Adds extras to the Intent that are needed by Herb.
     */
    public static void updateHerbIntent(Context context, Intent newIntent, Uri uri) {
        // For Elderberry flavored Herbs that are to be launched in a separate task, add a random
        // UUID to try and prevent Android from refocusing/clobbering items that share the same
        // base intent.  If we do support refocusing of existing Herbs, we need to do it on the
        // current URL and not the URL that it was triggered with.
        if (TextUtils.equals(
                FeatureUtilities.getHerbFlavor(), ChromeSwitches.HERB_FLAVOR_ELDERBERRY)
                && (newIntent.getFlags() & Intent.FLAG_ACTIVITY_NEW_TASK) != 0
                        || (newIntent.getFlags() & Intent.FLAG_ACTIVITY_NEW_DOCUMENT) != 0) {
            String uuid = UUID.randomUUID().toString();
            newIntent.addFlags(Intent.FLAG_ACTIVITY_MULTIPLE_TASK);
            newIntent.setFlags(
                    newIntent.getFlags() & ~Intent.FLAG_ACTIVITY_EXCLUDE_FROM_RECENTS);

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
                // Force a new document L+ to ensure the proper task/stack creation.
                newIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_DOCUMENT);
                newIntent.setClassName(context, SeparateTaskCustomTabActivity.class.getName());
            } else {
                int activityIndex =
                        ActivityAssigner.instance(ActivityAssigner.SEPARATE_TASK_CCT_NAMESPACE)
                        .assign(uuid);
                String className = SeparateTaskCustomTabActivity.class.getName() + activityIndex;
                newIntent.setClassName(context, className);
            }

            String url = IntentHandler.getUrlFromIntent(newIntent);
            assert url != null;
            newIntent.setData(new Uri.Builder().scheme(UrlConstants.CUSTOM_TAB_SCHEME)
                    .authority(uuid).query(url).build());
        }

        newIntent.putExtra(CustomTabsIntent.EXTRA_DEFAULT_SHARE_MENU_ITEM, true);
        newIntent.putExtra(CustomTabIntentDataProvider.EXTRA_IS_OPENED_BY_CHROME, true);
        newIntent.putExtra(CustomTabIntentDataProvider.EXTRA_SHOW_STAR_ICON, true);

        // Mark this as a trusted Chrome Intent.
        IntentHandler.addTrustedIntentExtras(newIntent, context);
    }

    /**
     * @return Whether the intent is for launching a Custom Tab.
     */
    public static boolean isCustomTabIntent(Intent intent) {
        if (intent == null || !intent.hasExtra(CustomTabsIntent.EXTRA_SESSION)) {
            return false;
        }
        return IntentHandler.getUrlFromIntent(intent) != null;
    }

    /**
     * Creates an Intent that can be used to launch a {@link CustomTabActivity}.
     */
    public static Intent createCustomTabActivityIntent(
            Context context, Intent intent, boolean addHerbExtras) {
        // Use the copy constructor to carry over the myriad of extras.
        Uri uri = Uri.parse(IntentHandler.getUrlFromIntent(intent));

        Intent newIntent = new Intent(intent);
        newIntent.setAction(Intent.ACTION_VIEW);
        newIntent.setClassName(context, CustomTabActivity.class.getName());
        newIntent.setData(uri);
        if (addHerbExtras) updateHerbIntent(context, newIntent, uri);

        return newIntent;
    }

    /**
     * Handles launching a {@link CustomTabActivity}, which will sit on top of a client's activity
     * in the same task.
     */
    private void launchCustomTabActivity() {
        boolean handled = CustomTabActivity.handleInActiveContentIfNeeded(getIntent());
        if (handled) return;

        // Create and fire a launch intent.
        startActivity(createCustomTabActivityIntent(
                this, getIntent(), !isCustomTabIntent(getIntent()) && mIsHerbIntent));
        if (mIsHerbIntent) overridePendingTransition(R.anim.activity_open_enter, R.anim.no_anim);
    }

    @SuppressLint("InlinedApi")
    private void launchTabbedMode() {
        maybePrefetchDnsInBackground();

        Intent newIntent = new Intent(getIntent());
        String className = MultiWindowUtils.getInstance().getTabbedActivityForIntent(
                newIntent, this).getName();
        newIntent.setClassName(getApplicationContext().getPackageName(), className);
        newIntent.setFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP | Intent.FLAG_ACTIVITY_NEW_TASK);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            newIntent.addFlags(Intent.FLAG_ACTIVITY_RETAIN_IN_RECENTS);
        }
        Uri uri = newIntent.getData();
        if (uri != null && "content".equals(uri.getScheme())) {
            newIntent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        }
        if (mIsInLegacyMultiInstanceMode) {
            MultiWindowUtils.getInstance().makeLegacyMultiInstanceIntent(this, newIntent);
        }

        // This system call is often modified by OEMs and not actionable. http://crbug.com/619646.
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
        StrictMode.allowThreadDiskWrites();
        try {
            startActivity(newIntent);
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }
    }

    /**
     * @return Whether there is already an browser instance of Chrome already running.
     */
    public boolean isChromeBrowserActivityRunning() {
        for (WeakReference<Activity> reference : ApplicationStatus.getRunningActivities()) {
            Activity activity = reference.get();
            if (activity == null) continue;

            String className = activity.getClass().getName();
            if (TextUtils.equals(className, ChromeTabbedActivity.class.getName())) {
                return true;
            }
        }
        return false;
    }

    /**
     * Tries to launch the First Run Experience.  If ChromeLauncherActivity is running with the
     * wrong Intent flags, we instead relaunch ChromeLauncherActivity to make sure it runs in its
     * own task, which then triggers First Run.
     * @return Whether or not the First Run Experience needed to be shown.
     */
    private boolean launchFirstRunExperience() {
        final Intent freIntent =
                FirstRunFlowSequencer.checkIfFirstRunIsNecessary(this, getIntent());
        if (freIntent == null) return false;

        if ((getIntent().getFlags() & Intent.FLAG_ACTIVITY_NEW_TASK) != 0) {
            startActivityForResult(freIntent, FIRST_RUN_EXPERIENCE_REQUEST_CODE);
        } else {
            Intent newIntent = new Intent(getIntent());
            newIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            startActivity(newIntent);
            finish();
        }
        return true;
    }

    /**
     * Records metrics gleaned from the Intent.
     */
    private void recordIntentMetrics() {
        Intent intent = getIntent();
        IntentHandler.ExternalAppId source =
                IntentHandler.determineExternalIntentSource(getPackageName(), intent);
        if (intent.getPackage() == null && source != IntentHandler.ExternalAppId.CHROME) {
            int flagsOfInterest = Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_NEW_DOCUMENT;
            int maskedFlags = intent.getFlags() & flagsOfInterest;
            sIntentFlagsHistogram.record(maskedFlags);
        }
        MediaNotificationUma.recordClickSource(intent);
    }
}
