// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.app.Activity;
import android.app.Fragment;
import android.app.PendingIntent;
import android.app.PendingIntent.CanceledException;
import android.content.Intent;
import android.os.Bundle;
import android.text.TextUtils;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.metrics.CachedMetrics.EnumeratedHistogramSample;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.metrics.UmaUtils;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettings;
import org.chromium.chrome.browser.preferences.datareduction.DataReductionPromoUtils;
import org.chromium.chrome.browser.preferences.datareduction.DataReductionProxyUma;
import org.chromium.chrome.browser.profiles.ProfileManagerUtils;
import org.chromium.chrome.browser.search_engines.TemplateUrlService;
import org.chromium.chrome.browser.searchwidget.SearchWidgetProvider;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.ui.base.LocalizationUtils;

import java.lang.reflect.Constructor;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.Callable;

/**
 * Handles the First Run Experience sequences shown to the user launching Chrome for the first time.
 * It supports only a simple format of FRE:
 *   [Welcome]
 *   [Intro pages...]
 *   [Sign-in page]
 * The activity might be run more than once, e.g. 1) for ToS and sign-in, and 2) for intro.
 */
public class FirstRunActivity extends AsyncInitializationActivity implements FirstRunPageDelegate {
    /** Alerted about various events when FirstRunActivity performs them. */
    public interface FirstRunActivityObserver {
        /** See {@link #onFlowIsKnown}. */
        void onFlowIsKnown(Bundle freProperties);

        /** See {@link #acceptTermsOfService}. */
        void onAcceptTermsOfService();

        /** See {@link #jumpToPage}. */
        void onJumpToPage(int position);

        /** Called when First Run is completed. */
        void onUpdateCachedEngineName();

        /** See {@link #abortFirstRunExperience}. */
        void onAbortFirstRunExperience();
    }

    protected static final String TAG = "FirstRunActivity";

    // Incoming parameters:
    public static final String EXTRA_COMING_FROM_CHROME_ICON = "Extra.ComingFromChromeIcon";
    public static final String EXTRA_USE_FRE_FLOW_SEQUENCER = "Extra.UseFreFlowSequencer";
    public static final String EXTRA_START_LIGHTWEIGHT_FRE = "Extra.StartLightweightFRE";
    public static final String EXTRA_CHROME_LAUNCH_INTENT = "Extra.FreChromeLaunchIntent";

    static final String SHOW_WELCOME_PAGE = "ShowWelcome";
    static final String SHOW_DATA_REDUCTION_PAGE = "ShowDataReduction";
    static final String SHOW_SEARCH_ENGINE_PAGE = "ShowSearchEnginePage";
    static final String SHOW_SIGNIN_PAGE = "ShowSignIn";

    static final String POST_NATIVE_SETUP_NEEDED = "PostNativeSetupNeeded";

    // Outgoing results:
    public static final String RESULT_SIGNIN_ACCOUNT_NAME = "ResultSignInTo";
    public static final String RESULT_SHOW_SIGNIN_SETTINGS = "ResultShowSignInSettings";
    public static final String EXTRA_FIRST_RUN_ACTIVITY_RESULT = "Extra.FreActivityResult";
    public static final String EXTRA_FIRST_RUN_COMPLETE = "Extra.FreComplete";

    public static final boolean DEFAULT_METRICS_AND_CRASH_REPORTING = true;

    // UMA constants.
    private static final int SIGNIN_SETTINGS_DEFAULT_ACCOUNT = 0;
    private static final int SIGNIN_SETTINGS_ANOTHER_ACCOUNT = 1;
    private static final int SIGNIN_ACCEPT_DEFAULT_ACCOUNT = 2;
    private static final int SIGNIN_ACCEPT_ANOTHER_ACCOUNT = 3;
    private static final int SIGNIN_NO_THANKS = 4;
    private static final int SIGNIN_MAX = 5;
    private static final EnumeratedHistogramSample sSigninChoiceHistogram =
            new EnumeratedHistogramSample("MobileFre.SignInChoice", SIGNIN_MAX);

    private static final int FRE_PROGRESS_STARTED = 0;
    private static final int FRE_PROGRESS_WELCOME_SHOWN = 1;
    private static final int FRE_PROGRESS_DATA_SAVER_SHOWN = 2;
    private static final int FRE_PROGRESS_SIGNIN_SHOWN = 3;
    private static final int FRE_PROGRESS_COMPLETED_SIGNED_IN = 4;
    private static final int FRE_PROGRESS_COMPLETED_NOT_SIGNED_IN = 5;
    private static final int FRE_PROGRESS_DEFAULT_SEARCH_ENGINE_SHOWN = 6;
    private static final int FRE_PROGRESS_MAX = 7;
    private static final EnumeratedHistogramSample sMobileFreProgressMainIntentHistogram =
            new EnumeratedHistogramSample("MobileFre.Progress.MainIntent", FRE_PROGRESS_MAX);
    private static final EnumeratedHistogramSample sMobileFreProgressViewIntentHistogram =
            new EnumeratedHistogramSample("MobileFre.Progress.ViewIntent", FRE_PROGRESS_MAX);

    private static FirstRunActivityObserver sObserver;

    private boolean mShowWelcomePage = true;

    private String mResultSignInAccountName;
    private boolean mResultIsDefaultAccount;
    private boolean mResultShowSignInSettings;

    private boolean mFlowIsKnown;
    private boolean mPostNativePageSequenceCreated;
    private boolean mNativeSideIsInitialized;
    private Set<FirstRunPage> mPagesToNotifyOfNativeInit;
    private boolean mDeferredCompleteFRE;

    private FirstRunViewPager mPager;

    private FirstRunFlowSequencer mFirstRunFlowSequencer;

    protected Bundle mFreProperties;

    private List<Callable<FirstRunPage>> mPages;

    private List<Integer> mFreProgressStates;

    /**
     * The pager adapter, which provides the pages to the view pager widget.
     */
    private FirstRunPagerAdapter mPagerAdapter;

    /**
     * Defines a sequence of pages to be shown (depending on parameters etc).
     */
    private void createPageSequence() {
        mPages = new ArrayList<Callable<FirstRunPage>>();
        mFreProgressStates = new ArrayList<Integer>();

        // An optional welcome page.
        if (mShowWelcomePage) {
            mPages.add(pageOf(ToSAndUMAFirstRunFragment.class));
            mFreProgressStates.add(FRE_PROGRESS_WELCOME_SHOWN);
        }

        // Other pages will be created by createPostNativePageSequence() after
        // native has been initialized.
    }

    private void createPostNativePageSequence() {
        // Note: Can't just use POST_NATIVE_SETUP_NEEDED for the early return, because this
        // populates |mPages| which needs to be done even even if onNativeInitialized() was
        // performed in a previous session.
        if (mPostNativePageSequenceCreated) return;
        mFirstRunFlowSequencer.onNativeInitialized(mFreProperties);

        boolean notifyAdapter = false;
        // An optional Data Saver page.
        if (mFreProperties.getBoolean(SHOW_DATA_REDUCTION_PAGE)) {
            mPages.add(pageOf(DataReductionProxyFirstRunFragment.class));
            mFreProgressStates.add(FRE_PROGRESS_DATA_SAVER_SHOWN);
            notifyAdapter = true;
        }

        // An optional page to select a default search engine.
        if (mFreProperties.getBoolean(SHOW_SEARCH_ENGINE_PAGE)
                && mFirstRunFlowSequencer.shouldShowSearchEnginePage()) {
            mPages.add(pageOf(DefaultSearchEngineFirstRunFragment.class));
            mFreProgressStates.add(FRE_PROGRESS_DEFAULT_SEARCH_ENGINE_SHOWN);
            notifyAdapter = true;
        }

        // An optional sign-in page.
        if (mFreProperties.getBoolean(SHOW_SIGNIN_PAGE)) {
            mPages.add(pageOf(AccountFirstRunFragment.class));
            mFreProgressStates.add(FRE_PROGRESS_SIGNIN_SHOWN);
            notifyAdapter = true;
        }

        if (notifyAdapter && mPagerAdapter != null) {
            mPagerAdapter.notifyDataSetChanged();
        }
        mPostNativePageSequenceCreated = true;
    }

    @Override
    protected boolean requiresFirstRunToBeCompleted(Intent intent) {
        // The user is already in First Run.
        return false;
    }

    @Override
    protected Bundle transformSavedInstanceStateForOnCreate(Bundle savedInstanceState) {
        // We pass null to Activity.onCreate() so that it doesn't automatically restore
        // the FragmentManager state - as that may cause fragments to be loaded that have
        // dependencies on native before native has been loaded (and then crash). Instead,
        // these fragments will be recreated manually by us and their progression restored
        // from |mFreProperties| which we still get from getSavedInstanceState() below.
        return null;
    }

    @Override
    public void setContentView() {
        Bundle savedInstanceState = getSavedInstanceState();
        if (savedInstanceState != null) {
            mFreProperties = savedInstanceState;
        } else if (getIntent() != null) {
            mFreProperties = getIntent().getExtras();
        } else {
            mFreProperties = new Bundle();
        }

        setFinishOnTouchOutside(true);

        // Skip creating content view if it is to start a lightweight First Run Experience.
        if (mFreProperties.getBoolean(FirstRunActivity.EXTRA_START_LIGHTWEIGHT_FRE)) {
            return;
        }

        mPager = new FirstRunViewPager(this);
        mPager.setId(R.id.fre_pager);
        mPager.setOffscreenPageLimit(3);
        setContentView(mPager);

        mFirstRunFlowSequencer = new FirstRunFlowSequencer(this, mFreProperties) {
            @Override
            public void onFlowIsKnown(Bundle freProperties) {
                mFlowIsKnown = true;
                if (freProperties == null) {
                    completeFirstRunExperience();
                    return;
                }

                mFreProperties = freProperties;
                mShowWelcomePage = mFreProperties.getBoolean(SHOW_WELCOME_PAGE);
                if (TextUtils.isEmpty(mResultSignInAccountName)) {
                    mResultSignInAccountName = mFreProperties.getString(
                            AccountFirstRunFragment.FORCE_SIGNIN_ACCOUNT_TO);
                }

                createPageSequence();
                if (mNativeSideIsInitialized) {
                    createPostNativePageSequence();
                }

                if (mPages.size() == 0) {
                    completeFirstRunExperience();
                    return;
                }

                mPagerAdapter =
                        new FirstRunPagerAdapter(getFragmentManager(), mPages, mFreProperties);
                stopProgressionIfNotAcceptedTermsOfService();
                mPager.setAdapter(mPagerAdapter);

                if (mNativeSideIsInitialized) {
                    skipPagesIfNecessary();
                }

                if (sObserver != null) sObserver.onFlowIsKnown(mFreProperties);
                recordFreProgressHistogram(mFreProgressStates.get(0));
            }
        };
        mFirstRunFlowSequencer.start();

        recordFreProgressHistogram(FRE_PROGRESS_STARTED);
    }

    @Override
    public void finishNativeInitialization() {
        super.finishNativeInitialization();

        Runnable onNativeFinished = new Runnable() {
            @Override
            public void run() {
                if (isActivityDestroyed()) return;

                onNativeDependenciesFullyInitialized();
            }
        };
        TemplateUrlService.getInstance().runWhenLoaded(onNativeFinished);
    }

    private void onNativeDependenciesFullyInitialized() {
        mNativeSideIsInitialized = true;
        if (mDeferredCompleteFRE) {
            completeFirstRunExperience();
            mDeferredCompleteFRE = false;
        } else if (mFlowIsKnown) {
            // Note: If mFlowIsKnown is false, then we're not ready to create the post native page
            // sequence - in that case this will be done when onFlowIsKnown() gets called.
            createPostNativePageSequence();
            if (mPagesToNotifyOfNativeInit != null) {
                for (FirstRunPage page : mPagesToNotifyOfNativeInit) {
                    page.onNativeInitialized();
                }
            }
            mPagesToNotifyOfNativeInit = null;
            skipPagesIfNecessary();
        }
    }

    @Override
    public boolean shouldStartGpuProcess() {
        return true;
    }

    // Activity:

    @Override
    public void onAttachFragment(Fragment fragment) {
        if (!(fragment instanceof FirstRunPage)) return;

        FirstRunPage page = (FirstRunPage) fragment;
        if (mNativeSideIsInitialized) {
            page.onNativeInitialized();
            return;
        }

        if (mPagesToNotifyOfNativeInit == null) {
            mPagesToNotifyOfNativeInit = new HashSet<FirstRunPage>();
        }
        mPagesToNotifyOfNativeInit.add(page);
    }

    @Override
    protected void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        outState.putAll(mFreProperties);
    }

    @Override
    public void onRestoreInstanceState(Bundle state) {
        // Don't automatically restore state here. This is a counterpart to the override
        // of transformSavedInstanceStateForOnCreate() as the two need to be consistent.
        // The default implementation of this would restore the state of the views, which
        // would otherwise cause a crash in ViewPager used to manage fragments - as it
        // expects consistency between the states restored by onCreate() and this method.
        // Activity doesn't check for null on the parameter, so pass an empty bundle.
        super.onRestoreInstanceState(new Bundle());
    }

    @Override
    public void onPause() {
        super.onPause();
        flushPersistentData();
    }

    @Override
    public void onStart() {
        super.onStart();
        // Since the FRE may be shown before any tab is shown, mark that this is the point at
        // which Chrome went to foreground. This is needed as otherwise an assert will be hit
        // in UmaUtils.getForegroundStartTime() when recording the time taken to load the first
        // page (which happens after native has been initialized possibly while FRE is still
        // active).
        UmaUtils.recordForegroundStartTime();
        stopProgressionIfNotAcceptedTermsOfService();
    }

    @Override
    public void onBackPressed() {
        // Terminate if we are still waiting for the native or for Android EDU / GAIA Child checks.
        if (mPagerAdapter == null) {
            abortFirstRunExperience();
            return;
        }

        Object currentItem = mPagerAdapter.instantiateItem(mPager, mPager.getCurrentItem());
        if (currentItem instanceof FirstRunPage) {
            FirstRunPage page = (FirstRunPage) currentItem;
            if (page.interceptBackPressed()) return;
        }

        if (mPager.getCurrentItem() == 0) {
            abortFirstRunExperience();
        } else {
            mPager.setCurrentItem(mPager.getCurrentItem() - 1, false);
        }
    }

    // FirstRunPageDelegate:
    @Override
    public void advanceToNextPage() {
        jumpToPage(mPager.getCurrentItem() + 1);
    }

    @Override
    public void recreateCurrentPage() {
        mPagerAdapter.notifyDataSetChanged();
    }

    @Override
    public void abortFirstRunExperience() {
        finish();

        sendPendingIntentIfNecessary(false);
        if (sObserver != null) sObserver.onAbortFirstRunExperience();
    }

    @Override
    public void completeFirstRunExperience() {
        if (!mNativeSideIsInitialized) {
            mDeferredCompleteFRE = true;
            return;
        }
        if (!TextUtils.isEmpty(mResultSignInAccountName)) {
            final int choice;
            if (mResultShowSignInSettings) {
                choice = mResultIsDefaultAccount ? SIGNIN_SETTINGS_DEFAULT_ACCOUNT
                                                 : SIGNIN_SETTINGS_ANOTHER_ACCOUNT;
            } else {
                choice = mResultIsDefaultAccount ? SIGNIN_ACCEPT_DEFAULT_ACCOUNT
                                                 : SIGNIN_ACCEPT_ANOTHER_ACCOUNT;
            }
            sSigninChoiceHistogram.record(choice);
            recordFreProgressHistogram(FRE_PROGRESS_COMPLETED_SIGNED_IN);
        } else {
            recordFreProgressHistogram(FRE_PROGRESS_COMPLETED_NOT_SIGNED_IN);
        }

        mFreProperties.putString(RESULT_SIGNIN_ACCOUNT_NAME, mResultSignInAccountName);
        mFreProperties.putBoolean(RESULT_SHOW_SIGNIN_SETTINGS, mResultShowSignInSettings);
        FirstRunFlowSequencer.markFlowAsCompleted(this, mFreProperties);

        if (DataReductionPromoUtils.getDisplayedFreOrSecondRunPromo()) {
            if (DataReductionProxySettings.getInstance().isDataReductionProxyEnabled()) {
                DataReductionProxyUma
                        .dataReductionProxyUIAction(DataReductionProxyUma.ACTION_FRE_ENABLED);
                DataReductionPromoUtils.saveFrePromoOptOut(false);
            } else {
                DataReductionProxyUma
                        .dataReductionProxyUIAction(DataReductionProxyUma.ACTION_FRE_DISABLED);
                DataReductionPromoUtils.saveFrePromoOptOut(true);
            }
        }

        // Update the search engine name cached by the widget.
        SearchWidgetProvider.updateCachedEngineName();
        if (sObserver != null) sObserver.onUpdateCachedEngineName();

        if (!sendPendingIntentIfNecessary(true)) {
            finish();
        } else {
            ApplicationStatus.registerStateListenerForActivity(new ActivityStateListener() {
                @Override
                public void onActivityStateChange(Activity activity, int newState) {
                    if (newState == ActivityState.STOPPED || newState == ActivityState.DESTROYED) {
                        finish();
                        ApplicationStatus.unregisterActivityStateListener(this);
                    }
                }
            }, this);
        }
    }

    @Override
    public void refuseSignIn() {
        sSigninChoiceHistogram.record(SIGNIN_NO_THANKS);
        mResultSignInAccountName = null;
        mResultShowSignInSettings = false;
    }

    @Override
    public void acceptSignIn(String accountName, boolean isDefaultAccount) {
        mResultSignInAccountName = accountName;
        mResultIsDefaultAccount = isDefaultAccount;
    }

    @Override
    public void askToOpenSignInSettings() {
        mResultShowSignInSettings = true;
    }

    @Override
    public boolean didAcceptTermsOfService() {
        boolean result = FirstRunUtils.didAcceptTermsOfService(getApplicationContext());
        if (sObserver != null) sObserver.onAcceptTermsOfService();
        return result;
    }

    @Override
    public void acceptTermsOfService(boolean allowCrashUpload) {
        // If default is true then it corresponds to opt-out and false corresponds to opt-in.
        UmaUtils.recordMetricsReportingDefaultOptIn(!DEFAULT_METRICS_AND_CRASH_REPORTING);
        FirstRunUtils.acceptTermsOfService(allowCrashUpload);
        FirstRunStatus.setSkipWelcomePage(true);
        flushPersistentData();
        stopProgressionIfNotAcceptedTermsOfService();
        jumpToPage(mPager.getCurrentItem() + 1);
    }

    protected void flushPersistentData() {
        if (mNativeSideIsInitialized) {
            ProfileManagerUtils.flushPersistentDataForAllProfiles();
        }
    }

    /**
     * Sends the PendingIntent included with the CHROME_LAUNCH_INTENT extra if it exists.
     * @param complete Whether first run completed successfully.
     * @return Whether a pending intent was sent.
     */
    protected final boolean sendPendingIntentIfNecessary(final boolean complete) {
        PendingIntent pendingIntent = IntentUtils.safeGetParcelableExtra(getIntent(),
                EXTRA_CHROME_LAUNCH_INTENT);
        if (pendingIntent == null) return false;

        Intent extraDataIntent = new Intent();
        extraDataIntent.putExtra(FirstRunActivity.EXTRA_FIRST_RUN_ACTIVITY_RESULT, true);
        extraDataIntent.putExtra(FirstRunActivity.EXTRA_FIRST_RUN_COMPLETE, complete);

        try {
            // After the PendingIntent has been sent, send a first run callback to custom tabs if
            // necessary.
            PendingIntent.OnFinished onFinished = new PendingIntent.OnFinished() {
                @Override
                public void onSendFinished(PendingIntent pendingIntent, Intent intent,
                        int resultCode, String resultData, Bundle resultExtras) {
                    if (ChromeLauncherActivity.isCustomTabIntent(intent)) {
                        CustomTabsConnection.getInstance().sendFirstRunCallbackIfNecessary(
                                intent, complete);
                    }
                }
            };

            // Use the PendingIntent to send the intent that originally launched Chrome. The intent
            // will go back to the ChromeLauncherActivity, which will route it accordingly.
            pendingIntent.send(this, complete ? Activity.RESULT_OK : Activity.RESULT_CANCELED,
                    extraDataIntent, onFinished, null);
            return true;
        } catch (CanceledException e) {
            Log.e(TAG, "Unable to send PendingIntent.", e);
        }
        return false;
    }

    /**
     * Transitions to a given page.
     * @return Whether the transition to a given page was allowed.
     * @param position A page index to transition to.
     */
    private boolean jumpToPage(int position) {
        if (sObserver != null) sObserver.onJumpToPage(position);

        if (mShowWelcomePage && !didAcceptTermsOfService()) {
            return position == 0;
        }
        if (position >= mPagerAdapter.getCount()) {
            completeFirstRunExperience();
            return false;
        }
        mPager.setCurrentItem(position, false);
        recordFreProgressHistogram(mFreProgressStates.get(position));
        return true;
    }

    private void stopProgressionIfNotAcceptedTermsOfService() {
        if (mPagerAdapter == null) return;
        mPagerAdapter.setStopAtTheFirstPage(mShowWelcomePage && !didAcceptTermsOfService());
    }

    private void skipPagesIfNecessary() {
        if (mPagerAdapter == null) return;

        int currentPageIndex = mPager.getCurrentItem();
        while (currentPageIndex < mPagerAdapter.getCount()) {
            FirstRunPage currentPage = (FirstRunPage) mPagerAdapter.getItem(currentPageIndex);
            if (!currentPage.shouldSkipPageOnCreate(getApplicationContext())) return;
            if (!jumpToPage(currentPageIndex + 1)) return;
            currentPageIndex = mPager.getCurrentItem();
        }
    }

    private void recordFreProgressHistogram(int state) {
        if (mFreProperties.getBoolean(FirstRunActivity.EXTRA_COMING_FROM_CHROME_ICON)) {
            sMobileFreProgressMainIntentHistogram.record(state);
        } else {
            sMobileFreProgressViewIntentHistogram.record(state);
        }
    }

    /**
     * Creates a trivial page constructor for a given page type.
     * @param clazz The .class of the page type.
     * @return The simple constructor for a given page type (no parameters, no tuning).
     */
    public static Callable<FirstRunPage> pageOf(final Class<? extends FirstRunPage> clazz) {
        return new Callable<FirstRunPage>() {
            @Override
            public FirstRunPage call() throws Exception {
                Constructor<? extends FirstRunPage> constructor = clazz.getDeclaredConstructor();
                return constructor.newInstance();
            }
        };
    }

    @Override
    public void showInfoPage(int url) {
        CustomTabActivity.showInfoPage(
                this, LocalizationUtils.substituteLocalePlaceholder(getString(url)));
    }

    @VisibleForTesting
    public static void setObserverForTest(FirstRunActivityObserver observer) {
        assert sObserver == null;
        sObserver = observer;
    }
}
