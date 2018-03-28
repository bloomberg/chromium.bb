// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.Notification;
import android.app.SearchManager;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.StrictMode;
import android.support.annotation.IntDef;
import android.support.annotation.Nullable;
import android.support.customtabs.CustomTabsIntent;
import android.support.customtabs.CustomTabsSessionToken;
import android.support.customtabs.TrustedWebUtils;

import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.StrictModeContext;
import org.chromium.base.metrics.CachedMetrics;
import org.chromium.chrome.browser.browserservices.BrowserSessionContentUtils;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.customtabs.SeparateTaskCustomTabActivity;
import org.chromium.chrome.browser.firstrun.FirstRunFlowSequencer;
import org.chromium.chrome.browser.instantapps.InstantAppsHandler;
import org.chromium.chrome.browser.metrics.MediaNotificationUma;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.notifications.NotificationPlatformBridge;
import org.chromium.chrome.browser.partnercustomizations.PartnerBrowserCustomizations;
import org.chromium.chrome.browser.searchwidget.SearchActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.DocumentModeAssassin;
import org.chromium.chrome.browser.upgrade.UpgradeActivity;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.chrome.browser.vr.CustomTabVrActivity;
import org.chromium.chrome.browser.vr_shell.VrIntentUtils;
import org.chromium.chrome.browser.webapps.ActivityAssigner;
import org.chromium.chrome.browser.webapps.WebappActivity;
import org.chromium.chrome.browser.webapps.WebappInfo;
import org.chromium.chrome.browser.webapps.WebappLauncherActivity;
import org.chromium.ui.widget.Toast;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.UUID;

/**
 * Dispatches incoming intents to the appropriate activity based on the current configuration and
 * Intent fired.
 */
public class LaunchIntentDispatcher implements IntentHandler.IntentHandlerDelegate {
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

    private static final String TAG = "ActivitiyDispatcher";

    /**
     * Timeout in ms for reading PartnerBrowserCustomizations provider. We do not trust third party
     * provider by default.
     */
    private static final int PARTNER_BROWSER_CUSTOMIZATIONS_TIMEOUT_MS = 10000;

    private static final CachedMetrics.SparseHistogramSample sIntentFlagsHistogram =
            new CachedMetrics.SparseHistogramSample("Launch.IntentFlags");

    private final Activity mActivity;
    private final Intent mIntent;
    private final boolean mIsCustomTabIntent;
    private final boolean mIsVrIntent;

    @IntDef({Action.CONTINUE, Action.FINISH_ACTIVITY, Action.FINISH_ACTIVITY_REMOVE_TASK})
    @Retention(RetentionPolicy.SOURCE)
    public @interface Action {
        int CONTINUE = 0;
        int FINISH_ACTIVITY = 1;
        int FINISH_ACTIVITY_REMOVE_TASK = 2;
    }

    /**
     * Dispatches the intent in the context of the activity.
     * In most cases calling this method will result in starting a new activity, in which case
     * the current activity will need to be finished as per the action returned.
     *
     * @param currentActivity activity that received the intent
     * @param intent intent to dispatch
     * @return action to take
     */
    public static @Action int dispatch(Activity currentActivity, Intent intent) {
        return new LaunchIntentDispatcher(currentActivity, intent).dispatch();
    }

    /**
     * Dispatches the intent to proper tabbed activity.
     * This method is similar to {@link #dispatch()}, but only handles intents that result in
     * starting a tabbed activity (i.e. one of *TabbedActivity classes).
     *
     * @param currentActivity activity that received the intent
     * @param intent intent to dispatch
     * @return action to take
     */
    public static @Action int dispatchToTabbedActivity(Activity currentActivity, Intent intent) {
        return new LaunchIntentDispatcher(currentActivity, intent).dispatchToTabbedActivity();
    }

    /**
     * Dispatches the intent to proper tabbed activity.
     * This method is similar to {@link #dispatch()}, but only handles intents that result in
     * starting a custom tab activity.
     */
    public static @Action int dispatchToCustomTabActivity(Activity currentActivity, Intent intent) {
        LaunchIntentDispatcher dispatcher = new LaunchIntentDispatcher(currentActivity, intent);
        if (!dispatcher.mIsCustomTabIntent) {
            return Action.CONTINUE;
        }
        dispatcher.launchCustomTabActivity();
        return Action.FINISH_ACTIVITY;
    }

    private LaunchIntentDispatcher(Activity activity, Intent intent) {
        mActivity = activity;
        mIntent = IntentUtils.sanitizeIntent(intent);

        // Needs to be called as early as possible, to accurately capture the
        // time at which the intent was received.
        if (mIntent != null && IntentHandler.getTimestampFromIntent(mIntent) == -1) {
            IntentHandler.addTimestampToIntent(mIntent);
        }

        recordIntentMetrics();

        mIsVrIntent = VrIntentUtils.isVrIntent(mIntent);
        boolean isCustomTabIntent = (!mIsVrIntent && isCustomTabIntent(mIntent))
                || (mIsVrIntent && VrIntentUtils.isCustomTabVrIntent(mIntent));
        mIsCustomTabIntent = isCustomTabIntent;
    }

    /**
     * Returns the options that should be used to start an activity.
     */
    @Nullable
    private Bundle getStartActivityIntentOptions() {
        Bundle options = null;
        if (mIsVrIntent) {
            // These options hide the 2D screenshot while we prepare for VR rendering.
            options = VrIntentUtils.getVrIntentOptions(mActivity);
        }
        return options;
    }

    /**
     * Figure out how to route the Intent.  Because this is on the critical path to startup, please
     * avoid making the pathway any more complicated than it already is.  Make sure that anything
     * you add _absolutely has_ to be here.
     */
    private @Action int dispatch() {
        // Read partner browser customizations information asynchronously.
        // We want to initialize early because when there are no tabs to restore, we should possibly
        // show homepage, which might require reading PartnerBrowserCustomizations provider.
        PartnerBrowserCustomizations.initializeAsync(
                mActivity.getApplicationContext(), PARTNER_BROWSER_CUSTOMIZATIONS_TIMEOUT_MS);

        int tabId = IntentUtils.safeGetIntExtra(
                mIntent, IntentHandler.TabOpenType.BRING_TAB_TO_FRONT.name(), Tab.INVALID_TAB_ID);
        boolean incognito =
                mIntent.getBooleanExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, false);

        // Check if a web search Intent is being handled.
        IntentHandler intentHandler = new IntentHandler(this, mActivity.getPackageName());
        String url = IntentHandler.getUrlFromIntent(mIntent);
        if (url == null && tabId == Tab.INVALID_TAB_ID && !incognito
                && intentHandler.handleWebSearchIntent(mIntent)) {
            return Action.FINISH_ACTIVITY;
        }

        // Check if a LIVE WebappActivity has to be brought back to the foreground.  We can't
        // check for a dead WebappActivity because we don't have that information without a global
        // TabManager.  If that ever lands, code to bring back any Tab could be consolidated
        // here instead of being spread between ChromeTabbedActivity and ChromeLauncherActivity.
        // https://crbug.com/443772, https://crbug.com/522918
        if (WebappLauncherActivity.bringWebappToFront(tabId)) {
            return Action.FINISH_ACTIVITY_REMOVE_TASK;
        }

        // The notification settings cog on the flipped side of Notifications and in the Android
        // Settings "App Notifications" view will open us with a specific category.
        if (mIntent.hasCategory(Notification.INTENT_CATEGORY_NOTIFICATION_PREFERENCES)) {
            NotificationPlatformBridge.launchNotificationPreferences(mActivity, mIntent);
            return Action.FINISH_ACTIVITY;
        }

        // Check if we should launch an Instant App to handle the intent.
        if (InstantAppsHandler.getInstance().handleIncomingIntent(
                    mActivity, mIntent, mIsCustomTabIntent, false)) {
            return Action.FINISH_ACTIVITY;
        }

        // Ignore this VR intent if we can't handle it in Chrome.
        if (mIsVrIntent && !VrIntentUtils.canHandleVrIntent(mActivity)) {
            return Action.FINISH_ACTIVITY;
        }

        // Check if we should push the user through First Run.
        if (FirstRunFlowSequencer.launch(mActivity, mIntent, false /* requiresBroadcast */,
                    false /* preferLightweightFre */)) {
            return Action.FINISH_ACTIVITY;
        }

        // Check if we should launch the ChromeTabbedActivity.
        if (!mIsCustomTabIntent && !FeatureUtilities.isDocumentMode(mActivity)) {
            return dispatchToTabbedActivity();
        }

        // Check if we should launch a Custom Tab.
        if (mIsCustomTabIntent) {
            if (!mIntent.getBooleanExtra(
                        TrustedWebUtils.EXTRA_LAUNCH_AS_TRUSTED_WEB_ACTIVITY, false)
                    || !launchTrustedWebActivity()) {
                launchCustomTabActivity();
            }

            return Action.FINISH_ACTIVITY;
        }

        // Force a user to migrate to document mode, if necessary.
        if (DocumentModeAssassin.getInstance().isMigrationNecessary()) {
            Log.d(TAG, "Diverting to UpgradeActivity via " + mActivity.getClass().getName());
            UpgradeActivity.launchInstance(mActivity, mIntent);
            return Action.FINISH_ACTIVITY_REMOVE_TASK;
        }

        return Action.CONTINUE;
    }

    @Override
    public void processWebSearchIntent(String query) {
        Intent searchIntent = new Intent(Intent.ACTION_WEB_SEARCH);
        searchIntent.putExtra(SearchManager.QUERY, query);

        try (StrictModeContext unused = StrictModeContext.allowDiskReads()) {
            int resolvers =
                    ContextUtils.getApplicationContext()
                            .getPackageManager()
                            .queryIntentActivities(searchIntent, PackageManager.GET_RESOLVED_FILTER)
                            .size();
            if (resolvers == 0) {
                // Phone doesn't have a WEB_SEARCH action handler, open Search Activity with
                // the given query.
                Intent searchActivityIntent = new Intent(Intent.ACTION_MAIN);
                searchActivityIntent.setClass(
                        ContextUtils.getApplicationContext(), SearchActivity.class);
                searchActivityIntent.putExtra(SearchManager.QUERY, query);
                mActivity.startActivity(searchActivityIntent);
            } else {
                mActivity.startActivity(searchIntent);
            }
        }
    }

    @Override
    public void processUrlViewIntent(String url, String referer, String headers,
            IntentHandler.TabOpenType tabOpenType, String externalAppId, int tabIdToBringToFront,
            boolean hasUserGesture, Intent intent) {
        assert false;
    }

    /** When started with an intent, maybe pre-resolve the domain. */
    private void maybePrefetchDnsInBackground() {
        if (mIntent != null && Intent.ACTION_VIEW.equals(mIntent.getAction())) {
            String maybeUrl = IntentHandler.getUrlFromIntent(mIntent);
            if (maybeUrl != null) {
                WarmupManager.getInstance().maybePrefetchDnsForUrlInBackground(mActivity, maybeUrl);
            }
        }
    }

    /**
     * @return Whether the intent is for launching a Custom Tab.
     */
    public static boolean isCustomTabIntent(Intent intent) {
        if (intent == null) return false;
        if (CustomTabsIntent.shouldAlwaysUseBrowserUI(intent)
                || !intent.hasExtra(CustomTabsIntent.EXTRA_SESSION)) {
            return false;
        }
        return IntentHandler.getUrlFromIntent(intent) != null;
    }

    /**
     * Creates an Intent that can be used to launch a {@link CustomTabActivity}.
     */
    public static Intent createCustomTabActivityIntent(Context context, Intent intent) {
        // Use the copy constructor to carry over the myriad of extras.
        Uri uri = Uri.parse(IntentHandler.getUrlFromIntent(intent));

        Intent newIntent = new Intent(intent);
        newIntent.setAction(Intent.ACTION_VIEW);
        newIntent.setData(uri);
        newIntent.setClassName(context, CustomTabActivity.class.getName());

        // If |uri| is a content:// URI, we want to propagate the URI permissions. This can't be
        // achieved by simply adding the FLAG_GRANT_READ_URI_PERMISSION to the Intent, since the
        // data URI on the Intent isn't |uri|, it just has |uri| as a query parameter.
        if (uri != null && UrlConstants.CONTENT_SCHEME.equals(uri.getScheme())) {
            context.grantUriPermission(
                    context.getPackageName(), uri, Intent.FLAG_GRANT_READ_URI_PERMISSION);
        }

        if (CommandLine.getInstance().hasSwitch(ChromeSwitches.OPEN_CUSTOM_TABS_IN_NEW_TASK)) {
            newIntent.setFlags(newIntent.getFlags() | Intent.FLAG_ACTIVITY_NEW_TASK);
        }

        // If a CCT intent triggers First Run, then NEW_TASK will be automatically applied.  As
        // part of that, it will inherit the EXCLUDE_FROM_RECENTS bit from ChromeLauncherActivity,
        // so explicitly remove it to ensure the CCT does not get lost in recents.
        if ((newIntent.getFlags() & Intent.FLAG_ACTIVITY_NEW_TASK) != 0
                || (newIntent.getFlags() & Intent.FLAG_ACTIVITY_NEW_DOCUMENT) != 0) {
            newIntent.setFlags(newIntent.getFlags() & ~Intent.FLAG_ACTIVITY_EXCLUDE_FROM_RECENTS);
            String uuid = UUID.randomUUID().toString();
            newIntent.addFlags(Intent.FLAG_ACTIVITY_MULTIPLE_TASK);
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
                // Force a new document L+ to ensure the proper task/stack creation.
                newIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_DOCUMENT);
                if (VrIntentUtils.isVrIntent(intent)) {
                    newIntent.setClassName(context, CustomTabVrActivity.class.getName());
                } else {
                    newIntent.setClassName(context, SeparateTaskCustomTabActivity.class.getName());
                }
            } else {
                int activityIndex =
                        ActivityAssigner.instance(ActivityAssigner.SEPARATE_TASK_CCT_NAMESPACE)
                                .assign(uuid);
                String className = SeparateTaskCustomTabActivity.class.getName() + activityIndex;
                newIntent.setClassName(context, className);
            }

            String url = IntentHandler.getUrlFromIntent(newIntent);
            assert url != null;
            newIntent.setData(new Uri.Builder()
                                      .scheme(UrlConstants.CUSTOM_TAB_SCHEME)
                                      .authority(uuid)
                                      .query(url)
                                      .build());
        }
        IntentUtils.safeRemoveExtra(intent, CustomTabIntentDataProvider.EXTRA_IS_OPENED_BY_CHROME);

        return newIntent;
    }

    /**
     * Handles launching a {@link CustomTabActivity}, which will sit on top of a client's activity
     * in the same task.
     */
    private void launchCustomTabActivity() {
        boolean handled = BrowserSessionContentUtils.handleInActiveContentIfNeeded(mIntent);
        if (handled) return;

        maybePrefetchDnsInBackground();

        // Create and fire a launch intent.
        Intent launchIntent = createCustomTabActivityIntent(mActivity, mIntent);
        // Allow disk writes during startActivity() to avoid strict mode violations on some
        // Samsung devices, see https://crbug.com/796548.
        try (StrictModeContext smc = StrictModeContext.allowDiskWrites()) {
            mActivity.startActivity(launchIntent, getStartActivityIntentOptions());
        }
    }

    private boolean launchTrustedWebActivity() {
        CustomTabsSessionToken session = CustomTabsSessionToken.getSessionTokenFromIntent(mIntent);
        if (!CustomTabsConnection.getInstance().canSessionLaunchInTrustedWebActivity(
                    session, Uri.parse(mIntent.getDataString()))) {
            return false;
        }

        // TODO(yusufo): WebappInfo houses a lot of logic around preparing/easing out the initial
        // launch via extras for icons, splashscreens, screen orientation etc. We need a way to
        // plumb that information to Trusted Web Activities.
        WebappInfo info = WebappInfo.create(mIntent, session);
        if (info == null) return false;

        WebappActivity.addWebappInfo(info.id(), info);
        Intent launchIntent = WebappLauncherActivity.createWebappLaunchIntent(info, false);
        launchIntent.putExtras(mIntent.getExtras());

        mActivity.startActivity(launchIntent);
        return true;
    }

    /**
     * Handles launching a {@link ChromeTabbedActivity}.
     */
    @SuppressLint("InlinedApi")
    private @Action int dispatchToTabbedActivity() {
        maybePrefetchDnsInBackground();

        Intent newIntent = new Intent(mIntent);
        Class<?> tabbedActivityClass =
                MultiWindowUtils.getInstance().getTabbedActivityForIntent(newIntent, mActivity);
        newIntent.setClassName(
                mActivity.getApplicationContext().getPackageName(), tabbedActivityClass.getName());
        newIntent.setFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP | Intent.FLAG_ACTIVITY_NEW_TASK);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            newIntent.addFlags(Intent.FLAG_ACTIVITY_RETAIN_IN_RECENTS);
        }
        Uri uri = newIntent.getData();
        boolean isContentScheme = false;
        if (uri != null && UrlConstants.CONTENT_SCHEME.equals(uri.getScheme())) {
            isContentScheme = true;
            newIntent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        }
        if (MultiWindowUtils.getInstance().shouldRunInLegacyMultiInstanceMode(mActivity, mIntent)) {
            MultiWindowUtils.getInstance().makeLegacyMultiInstanceIntent(mActivity, newIntent);
        }

        if (newIntent.getComponent().getClassName().equals(mActivity.getClass().getName())) {
            // We're trying to start activity that is already running - just continue.
            return Action.CONTINUE;
        }

        // This system call is often modified by OEMs and not actionable. http://crbug.com/619646.
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskWrites();
        try {
            mActivity.startActivity(newIntent, getStartActivityIntentOptions());
        } catch (SecurityException ex) {
            if (isContentScheme) {
                Toast.makeText(mActivity,
                             org.chromium.chrome.R.string.external_app_restricted_access_error,
                             Toast.LENGTH_LONG)
                        .show();
            } else {
                throw ex;
            }
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }

        return Action.FINISH_ACTIVITY;
    }

    /**
     * Records metrics gleaned from the Intent.
     */
    private void recordIntentMetrics() {
        IntentHandler.ExternalAppId source = IntentHandler.determineExternalIntentSource(mIntent);
        if (mIntent.getPackage() == null && source != IntentHandler.ExternalAppId.CHROME) {
            int flagsOfInterest = Intent.FLAG_ACTIVITY_NEW_TASK;
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
                flagsOfInterest |= Intent.FLAG_ACTIVITY_NEW_DOCUMENT;
            }
            int maskedFlags = mIntent.getFlags() & flagsOfInterest;
            sIntentFlagsHistogram.record(maskedFlags);
        }
        MediaNotificationUma.recordClickSource(mIntent);
    }
}
