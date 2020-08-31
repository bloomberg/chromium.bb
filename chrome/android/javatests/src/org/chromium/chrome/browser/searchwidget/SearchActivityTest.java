// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.searchwidget;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.Instrumentation;
import android.app.Instrumentation.ActivityMonitor;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.net.Uri;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;
import android.text.TextUtils;
import android.view.KeyEvent;
import android.view.ViewGroup;

import androidx.core.content.FileProvider;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.Callback;
import org.chromium.base.ContentUriUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.locale.DefaultSearchEngineDialogHelperUtils;
import org.chromium.chrome.browser.locale.DefaultSearchEnginePromoDialog;
import org.chromium.chrome.browser.locale.DefaultSearchEnginePromoDialog.DefaultSearchEnginePromoDialogObserver;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.omnibox.OmniboxSuggestionType;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteCoordinatorTestUtils;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteResult;
import org.chromium.chrome.browser.omnibox.suggestions.CachedZeroSuggestionsManager;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestion;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionBuilderForTest;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionsDropdown;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionView;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.searchwidget.SearchActivity.SearchActivityDelegate;
import org.chromium.chrome.browser.share.clipboard.ClipboardImageFileProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.MultiActivityTestRule;
import org.chromium.chrome.test.util.ActivityUtils;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.KeyUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TestTouchUtils;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.ui.base.Clipboard;
import org.chromium.url.GURL;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.TimeoutException;

/**
 * Tests the {@link SearchActivity}.
 *
 * TODO(dfalcantara): Add tests for:
 *                    + Performing a search query.
 *
 *                    + Performing a search query while the SearchActivity is alive and the
 *                      default search engine is changed outside the SearchActivity.
 *
 *                    + Add microphone tests somehow (vague query + confident query).
 */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class SearchActivityTest {
    private static final long OMNIBOX_SHOW_TIMEOUT_MS = 5000L;
    private static final String TEST_PNG_IMAGE_FILE_EXTENSION = ".png";

    @ParameterAnnotations.ClassParameter
    private static List<ParameterSet> sClassParams =
            Arrays.asList(new ParameterSet().value(false).name("DisableRecyclerView"),
                    new ParameterSet().value(true).name("EnableRecyclerView"));

    private static class TestDelegate
            extends SearchActivityDelegate implements DefaultSearchEnginePromoDialogObserver {
        public final CallbackHelper shouldDelayNativeInitializationCallback = new CallbackHelper();
        public final CallbackHelper showSearchEngineDialogIfNeededCallback = new CallbackHelper();
        public final CallbackHelper onFinishDeferredInitializationCallback = new CallbackHelper();
        public final CallbackHelper onPromoDialogShownCallback = new CallbackHelper();

        public boolean shouldDelayLoadingNative;
        public boolean shouldDelayDeferredInitialization;
        public boolean shouldShowRealSearchDialog;

        public DefaultSearchEnginePromoDialog shownPromoDialog;
        public Callback<Boolean> onSearchEngineFinalizedCallback;

        @Override
        boolean shouldDelayNativeInitialization() {
            shouldDelayNativeInitializationCallback.notifyCalled();
            return shouldDelayLoadingNative;
        }

        @Override
        void showSearchEngineDialogIfNeeded(
                Activity activity, Callback<Boolean> onSearchEngineFinalized) {
            onSearchEngineFinalizedCallback = onSearchEngineFinalized;
            showSearchEngineDialogIfNeededCallback.notifyCalled();

            if (shouldShowRealSearchDialog) {
                LocaleManager.setInstanceForTest(new LocaleManager() {
                    @Override
                    public int getSearchEnginePromoShowType() {
                        return SearchEnginePromoType.SHOW_EXISTING;
                    }

                    @Override
                    public List<TemplateUrl> getSearchEnginesForPromoDialog(int promoType) {
                        return TemplateUrlServiceFactory.get().getTemplateUrls();
                    }
                });
                super.showSearchEngineDialogIfNeeded(activity, onSearchEngineFinalized);
            } else {
                LocaleManager.setInstanceForTest(new LocaleManager() {
                    @Override
                    public boolean needToCheckForSearchEnginePromo() {
                        return false;
                    }
                });
                if (!shouldDelayDeferredInitialization) onSearchEngineFinalized.onResult(true);
            }
        }

        @Override
        public void onFinishDeferredInitialization() {
            onFinishDeferredInitializationCallback.notifyCalled();
        }

        @Override
        public void onDialogShown(DefaultSearchEnginePromoDialog dialog) {
            shownPromoDialog = dialog;
            onPromoDialogShownCallback.notifyCalled();
        }
    }

    // Helper class for clipboard Omnibox test.
    private class FileProviderHelper implements ContentUriUtils.FileProviderUtil {
        private static final String API_AUTHORITY_SUFFIX = ".FileProvider";

        @Override
        public Uri getContentUriFromFile(File file) {
            Context appContext = ContextUtils.getApplicationContext();
            return FileProvider.getUriForFile(
                    appContext, appContext.getPackageName() + API_AUTHORITY_SUFFIX, file);
        }
    }

    @Rule
    public MultiActivityTestRule mTestRule = new MultiActivityTestRule();

    @Rule
    public ChromeActivityTestRule<ChromeTabbedActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeTabbedActivity.class);

    @Mock
    VoiceRecognitionHandler mHandler;

    private TestDelegate mTestDelegate;
    private boolean mEnableRecyclerView;

    public SearchActivityTest(boolean enableRecyclerView) {
        mEnableRecyclerView = enableRecyclerView;
    }

    @Before
    public void setUp() {
        if (mEnableRecyclerView) {
            Features.getInstance().enable(ChromeFeatureList.OMNIBOX_SUGGESTIONS_RECYCLER_VIEW);
        } else {
            Features.getInstance().disable(ChromeFeatureList.OMNIBOX_SUGGESTIONS_RECYCLER_VIEW);
        }
        Features.ensureCommandLineIsUpToDate();

        MockitoAnnotations.initMocks(this);
        doReturn(true).when(mHandler).isVoiceSearchEnabled();

        mTestDelegate = new TestDelegate();
        SearchActivity.setDelegateForTests(mTestDelegate);
        DefaultSearchEnginePromoDialog.setObserverForTests(mTestDelegate);
    }

    @After
    public void tearDown() {
        SearchActivity.setDelegateForTests(null);
        LocaleManager.setInstanceForTest(null);
    }

    @Test
    @SmallTest
    public void testOmniboxSuggestionContainerAppears() throws Exception {
        SearchActivity searchActivity = startSearchActivity();

        // Wait for the Activity to fully load.
        mTestDelegate.shouldDelayNativeInitializationCallback.waitForCallback(0);
        mTestDelegate.showSearchEngineDialogIfNeededCallback.waitForCallback(0);
        mTestDelegate.onFinishDeferredInitializationCallback.waitForCallback(0);

        // Type in anything.  It should force the suggestions to appear.
        setUrlBarText(searchActivity, "anything.");
        final SearchActivityLocationBarLayout locationBar =
                (SearchActivityLocationBarLayout) searchActivity.findViewById(
                        R.id.search_location_bar);
        OmniboxTestUtils.waitForOmniboxSuggestions(locationBar);
    }

    @Test
    @SmallTest
    public void testStartsBrowserAfterUrlSubmitted_aboutblank() throws Exception {
        verifyUrlLoads(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
    }

    @Test
    @SmallTest
    public void testStartsBrowserAfterUrlSubmitted_chromeUrl() throws Exception {
        verifyUrlLoads("chrome://flags/");
    }

    private void verifyUrlLoads(final String url) throws Exception {
        SearchActivity searchActivity = startSearchActivity();

        // Wait for the Activity to fully load.
        mTestDelegate.shouldDelayNativeInitializationCallback.waitForCallback(0);
        mTestDelegate.showSearchEngineDialogIfNeededCallback.waitForCallback(0);
        mTestDelegate.onFinishDeferredInitializationCallback.waitForCallback(0);

        // Monitor for ChromeTabbedActivity.
        final Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();
        waitForChromeTabbedActivityToStart(new Callable<Void>() {
            @Override
            public Void call() {
                // Type in a URL that should get kicked to ChromeTabbedActivity.
                setUrlBarText(searchActivity, url);
                final UrlBar urlBar = (UrlBar) searchActivity.findViewById(R.id.url_bar);
                KeyUtils.singleKeyEventView(instrumentation, urlBar, KeyEvent.KEYCODE_ENTER);
                return null;
            }
        }, url);
    }

    @Test
    @SmallTest
    public void testVoiceSearchBeforeNativeIsLoaded() throws Exception {
        // Wait for the activity to load, but don't let it load the native library.
        mTestDelegate.shouldDelayLoadingNative = true;
        final SearchActivity searchActivity = startSearchActivity(0, /*isVoiceSearch=*/true);
        final SearchActivityLocationBarLayout locationBar =
                (SearchActivityLocationBarLayout) searchActivity.findViewById(
                        R.id.search_location_bar);
        locationBar.setVoiceRecognitionHandlerForTesting(mHandler);
        locationBar.beginQuery(/* isVoiceSearchIntent= */ true, /* optionalText= */ null);
        verify(mHandler, times(0))
                .startVoiceRecognition(
                        VoiceRecognitionHandler.VoiceInteractionSource.SEARCH_WIDGET);

        mTestDelegate.shouldDelayNativeInitializationCallback.waitForCallback(0);
        Assert.assertEquals(0, mTestDelegate.showSearchEngineDialogIfNeededCallback.getCallCount());
        Assert.assertEquals(0, mTestDelegate.onFinishDeferredInitializationCallback.getCallCount());

        // Start loading native, then let the activity finish initialization.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> searchActivity.startDelayedNativeInitialization());

        Assert.assertEquals(
                1, mTestDelegate.shouldDelayNativeInitializationCallback.getCallCount());
        mTestDelegate.showSearchEngineDialogIfNeededCallback.waitForCallback(0);
        mTestDelegate.onFinishDeferredInitializationCallback.waitForCallback(0);

        verify(mHandler).startVoiceRecognition(
                VoiceRecognitionHandler.VoiceInteractionSource.SEARCH_WIDGET);
    }

    @Test
    @SmallTest
    public void testTypeBeforeNativeIsLoaded() throws Exception {
        // Wait for the activity to load, but don't let it load the native library.
        mTestDelegate.shouldDelayLoadingNative = true;
        final SearchActivity searchActivity = startSearchActivity();
        mTestDelegate.shouldDelayNativeInitializationCallback.waitForCallback(0);
        Assert.assertEquals(0, mTestDelegate.showSearchEngineDialogIfNeededCallback.getCallCount());
        Assert.assertEquals(0, mTestDelegate.onFinishDeferredInitializationCallback.getCallCount());

        // Set some text in the search box (but don't hit enter).
        setUrlBarText(searchActivity, ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);

        // Start loading native, then let the activity finish initialization.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> searchActivity.startDelayedNativeInitialization());

        Assert.assertEquals(
                1, mTestDelegate.shouldDelayNativeInitializationCallback.getCallCount());
        mTestDelegate.showSearchEngineDialogIfNeededCallback.waitForCallback(0);
        mTestDelegate.onFinishDeferredInitializationCallback.waitForCallback(0);

        // Omnibox suggestions should appear now.
        final SearchActivityLocationBarLayout locationBar =
                (SearchActivityLocationBarLayout) searchActivity.findViewById(
                        R.id.search_location_bar);
        OmniboxTestUtils.waitForOmniboxSuggestions(locationBar);

        final Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();
        waitForChromeTabbedActivityToStart(new Callable<Void>() {
            @Override
            public Void call() {
                // Hitting enter should submit the URL and kick the user to the browser.
                UrlBar urlBar = (UrlBar) searchActivity.findViewById(R.id.url_bar);
                KeyUtils.singleKeyEventView(instrumentation, urlBar, KeyEvent.KEYCODE_ENTER);
                return null;
            }
        }, ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
    }

    @Test
    @SmallTest
    @RetryOnFailure(message = "crbug.com/765476")
    public void testEnterUrlBeforeNativeIsLoaded() throws Exception {
        // Wait for the activity to load, but don't let it load the native library.
        mTestDelegate.shouldDelayLoadingNative = true;
        final SearchActivity searchActivity = startSearchActivity();
        mTestDelegate.shouldDelayNativeInitializationCallback.waitForCallback(0);
        Assert.assertEquals(0, mTestDelegate.showSearchEngineDialogIfNeededCallback.getCallCount());
        Assert.assertEquals(0, mTestDelegate.onFinishDeferredInitializationCallback.getCallCount());

        // Submit a URL before native is loaded.  The browser shouldn't start yet.
        final Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();
        setUrlBarText(searchActivity, ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
        UrlBar urlBar = (UrlBar) searchActivity.findViewById(R.id.url_bar);
        KeyUtils.singleKeyEventView(instrumentation, urlBar, KeyEvent.KEYCODE_ENTER);
        Assert.assertEquals(searchActivity, ApplicationStatus.getLastTrackedFocusedActivity());
        Assert.assertFalse(searchActivity.isFinishing());

        waitForChromeTabbedActivityToStart(new Callable<Void>() {
            @Override
            public Void call() throws TimeoutException {
                // Finish initialization.  It should notice the URL is queued up and start the
                // browser.
                TestThreadUtils.runOnUiThreadBlocking(
                        () -> { searchActivity.startDelayedNativeInitialization(); });

                Assert.assertEquals(
                        1, mTestDelegate.shouldDelayNativeInitializationCallback.getCallCount());
                mTestDelegate.showSearchEngineDialogIfNeededCallback.waitForCallback(0);
                mTestDelegate.onFinishDeferredInitializationCallback.waitForCallback(0);
                return null;
            }
        }, ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
    }

    @Test
    @SmallTest
    public void testZeroSuggestBeforeNativeIsLoaded() {
        LocaleManager.setInstanceForTest(new LocaleManager() {
            @Override
            public boolean needToCheckForSearchEnginePromo() {
                return false;
            }
        });

        // Cache some mock results to show.
        OmniboxSuggestion mockSuggestion =
                OmniboxSuggestionBuilderForTest.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .setDisplayText("https://google.com")
                        .setDescription("https://google.com")
                        .setUrl(new GURL("https://google.com"))
                        .build();
        OmniboxSuggestion mockSuggestion2 =
                OmniboxSuggestionBuilderForTest.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .setDisplayText("https://android.com")
                        .setDescription("https://android.com")
                        .setUrl(new GURL("https://android.com"))
                        .build();
        List<OmniboxSuggestion> list = new ArrayList<>();
        list.add(mockSuggestion);
        list.add(mockSuggestion2);
        AutocompleteResult data = new AutocompleteResult(list, null);
        CachedZeroSuggestionsManager.saveToCache(data);

        // Wait for the activity to load, but don't let it load the native library.
        mTestDelegate.shouldDelayLoadingNative = true;
        final SearchActivity searchActivity = startSearchActivity();

        // Focus on the url bar with not text.

        setUrlBarText(searchActivity, "");
        // Omnibox suggestions should appear now.
        final SearchActivityLocationBarLayout locationBar =
                (SearchActivityLocationBarLayout) searchActivity.findViewById(
                        R.id.search_location_bar);
        OmniboxTestUtils.waitForOmniboxSuggestions(locationBar, OMNIBOX_SHOW_TIMEOUT_MS);
    }

    @Test
    @SmallTest
    public void testTypeBeforeDeferredInitialization() throws Exception {
        // Start the Activity.  It should pause and assume that a promo dialog has appeared.
        mTestDelegate.shouldDelayDeferredInitialization = true;
        final SearchActivity searchActivity = startSearchActivity();
        mTestDelegate.shouldDelayNativeInitializationCallback.waitForCallback(0);
        mTestDelegate.showSearchEngineDialogIfNeededCallback.waitForCallback(0);
        Assert.assertNotNull(mTestDelegate.onSearchEngineFinalizedCallback);
        Assert.assertEquals(0, mTestDelegate.onFinishDeferredInitializationCallback.getCallCount());

        // Set some text in the search box, then continue startup.
        setUrlBarText(searchActivity, ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mTestDelegate.onSearchEngineFinalizedCallback.onResult(true));

        // Let the initialization finish completely.
        Assert.assertEquals(
                1, mTestDelegate.shouldDelayNativeInitializationCallback.getCallCount());
        Assert.assertEquals(1, mTestDelegate.showSearchEngineDialogIfNeededCallback.getCallCount());
        mTestDelegate.onFinishDeferredInitializationCallback.waitForCallback(0);

        // Omnibox suggestions should appear now.
        final SearchActivityLocationBarLayout locationBar =
                (SearchActivityLocationBarLayout) searchActivity.findViewById(
                        R.id.search_location_bar);
        OmniboxTestUtils.waitForOmniboxSuggestions(locationBar, OMNIBOX_SHOW_TIMEOUT_MS);

        final Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();
        waitForChromeTabbedActivityToStart(new Callable<Void>() {
            @Override
            public Void call() {
                // Hitting enter should submit the URL and kick the user to the browser.
                UrlBar urlBar = (UrlBar) searchActivity.findViewById(R.id.url_bar);
                KeyUtils.singleKeyEventView(instrumentation, urlBar, KeyEvent.KEYCODE_ENTER);
                return null;
            }
        }, ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
    }

    @Test
    @SmallTest
    @FlakyTest(message = "crbug.com/1075804")
    public void testRealPromoDialogInterruption() throws Exception {
        // Start the Activity.  It should pause when the promo dialog appears.
        mTestDelegate.shouldShowRealSearchDialog = true;
        final SearchActivity searchActivity = startSearchActivity();
        mTestDelegate.shouldDelayNativeInitializationCallback.waitForCallback(0);
        mTestDelegate.showSearchEngineDialogIfNeededCallback.waitForCallback(0);
        mTestDelegate.onPromoDialogShownCallback.waitForCallback(0);
        Assert.assertEquals(0, mTestDelegate.onFinishDeferredInitializationCallback.getCallCount());

        // Set some text in the search box, then select the first engine to continue startup.
        setUrlBarText(searchActivity, ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
        DefaultSearchEngineDialogHelperUtils.clickOnFirstEngine(
                mTestDelegate.shownPromoDialog.findViewById(android.R.id.content));

        // Let the initialization finish completely.
        Assert.assertEquals(
                1, mTestDelegate.shouldDelayNativeInitializationCallback.getCallCount());
        Assert.assertEquals(1, mTestDelegate.showSearchEngineDialogIfNeededCallback.getCallCount());
        mTestDelegate.onFinishDeferredInitializationCallback.waitForCallback(0);

        // Omnibox suggestions should appear now.
        final SearchActivityLocationBarLayout locationBar =
                (SearchActivityLocationBarLayout) searchActivity.findViewById(
                        R.id.search_location_bar);
        OmniboxTestUtils.waitForOmniboxSuggestions(locationBar);

        final Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();
        waitForChromeTabbedActivityToStart(new Callable<Void>() {
            @Override
            public Void call() {
                // Hitting enter should submit the URL and kick the user to the browser.
                UrlBar urlBar = (UrlBar) searchActivity.findViewById(R.id.url_bar);
                KeyUtils.singleKeyEventView(instrumentation, urlBar, KeyEvent.KEYCODE_ENTER);
                return null;
            }
        }, ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
    }

    @Test
    @SmallTest
    public void testRealPromoDialogDismissWithoutSelection() throws Exception {
        // Start the Activity.  It should pause when the promo dialog appears.
        mTestDelegate.shouldShowRealSearchDialog = true;
        SearchActivity activity = startSearchActivity();
        mTestDelegate.shouldDelayNativeInitializationCallback.waitForCallback(0);
        mTestDelegate.showSearchEngineDialogIfNeededCallback.waitForCallback(0);
        mTestDelegate.onPromoDialogShownCallback.waitForCallback(0);
        Assert.assertEquals(0, mTestDelegate.onFinishDeferredInitializationCallback.getCallCount());

        // Dismiss the dialog without acting on it.
        mTestDelegate.shownPromoDialog.dismiss();

        // SearchActivity should realize the failure case and prevent the user from using it.
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                List<Activity> activities = ApplicationStatus.getRunningActivities();
                if (activities.isEmpty()) return true;

                if (activities.size() != 1) {
                    updateFailureReason("Multiple non-destroyed activities: " + activities);
                    return false;
                }
                if (activities.get(0) != activity) {
                    updateFailureReason("Remaining activity is not the search activity under test: "
                            + activities.get(0));
                }
                updateFailureReason("Search activity has not called finish()");
                return activity.isFinishing();
            }
        });
        Assert.assertEquals(
                1, mTestDelegate.shouldDelayNativeInitializationCallback.getCallCount());
        Assert.assertEquals(1, mTestDelegate.showSearchEngineDialogIfNeededCallback.getCallCount());
        Assert.assertEquals(0, mTestDelegate.onFinishDeferredInitializationCallback.getCallCount());
    }

    @Test
    @SmallTest
    public void testNewIntentDiscardsQuery() {
        final SearchActivity searchActivity = startSearchActivity();
        setUrlBarText(searchActivity, "first query");
        final SearchActivityLocationBarLayout locationBar =
                (SearchActivityLocationBarLayout) searchActivity.findViewById(
                        R.id.search_location_bar);
        OmniboxTestUtils.waitForOmniboxSuggestions(locationBar, OMNIBOX_SHOW_TIMEOUT_MS);

        // Start the Activity again by firing another copy of the same Intent.
        SearchActivity restartedActivity = startSearchActivity(1, /*isVoiceSearch=*/false);
        Assert.assertEquals(searchActivity, restartedActivity);

        // The query should be wiped.
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                UrlBar urlBar = (UrlBar) searchActivity.findViewById(R.id.url_bar);
                return TextUtils.isEmpty(urlBar.getText());
            }
        });
    }

    @Test
    @SmallTest
    @Features.
    EnableFeatures({ChromeFeatureList.OMNIBOX_ENABLE_CLIPBOARD_PROVIDER_IMAGE_SUGGESTIONS})
    public void testImageSearch() throws InterruptedException, Exception {
        // Put an image into system clipboard.
        putAnImageIntoClipboard();

        // Start the Activity.
        final SearchActivity searchActivity = startSearchActivity();

        // Omnibox suggestions should appear now.
        final SearchActivityLocationBarLayout locationBar =
                (SearchActivityLocationBarLayout) searchActivity.findViewById(
                        R.id.search_location_bar);
        OmniboxTestUtils.waitForOmniboxSuggestions(locationBar, OMNIBOX_SHOW_TIMEOUT_MS);
        waitForSuggestionType(locationBar, OmniboxSuggestionType.CLIPBOARD_IMAGE);
        OmniboxSuggestionsDropdown suggestionsDropdown =
                AutocompleteCoordinatorTestUtils.getSuggestionsDropdown(
                        locationBar.getAutocompleteCoordinator());

        int imageSuggestionIndex = -1;
        // Find the index of the image clipboard suggestion.
        for (int i = 0; i < suggestionsDropdown.getItemCount(); ++i) {
            OmniboxSuggestion suggestion = AutocompleteCoordinatorTestUtils.getOmniboxSuggestionAt(
                    locationBar.getAutocompleteCoordinator(), i);
            if (suggestion != null
                    && suggestion.getType() == OmniboxSuggestionType.CLIPBOARD_IMAGE) {
                imageSuggestionIndex = i;
                break;
            }
        }
        Assert.assertNotEquals(
                "Cannot find the image clipboard Omnibox suggestion", -1, imageSuggestionIndex);

        OmniboxSuggestion imageSuggestion = AutocompleteCoordinatorTestUtils.getOmniboxSuggestionAt(
                locationBar.getAutocompleteCoordinator(), imageSuggestionIndex);
        Assert.assertNotNull("The image clipboard suggestion should contains post content type.",
                imageSuggestion.getPostContentType());
        Assert.assertNotEquals(
                "The image clipboard suggestion should not contains am empty post content type.", 0,
                imageSuggestion.getPostContentType().length());
        Assert.assertNotNull("The image clipboard suggestion should contains post data.",
                imageSuggestion.getPostData());
        Assert.assertNotEquals(
                "The image clipboard suggestion should not contains am empty post data.", 0,
                imageSuggestion.getPostData().length);

        // Find the index of clipboard suggestion in the dropdown list.
        final int clipboardSuggestionIndexInDropdown =
                AutocompleteCoordinatorTestUtils.getIndexForFirstSuggestionOfType(
                        locationBar.getAutocompleteCoordinator(),
                        OmniboxSuggestionUiType.CLIPBOARD_SUGGESTION);
        Assert.assertNotEquals("Cannot find the image clipboard Omnibox suggestion in UI.", -1,
                clipboardSuggestionIndexInDropdown);

        // Make sure the new tab is launched.
        final ChromeTabbedActivity cta = ActivityUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), ChromeTabbedActivity.class,
                new Callable<Void>() {
                    @Override
                    public Void call() throws InterruptedException {
                        clickSuggestionAt(suggestionsDropdown, clipboardSuggestionIndexInDropdown);
                        return null;
                    }
                });

        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                Tab tab = cta.getActivityTab();
                if (tab == null) return false;
                // Make sure tab is in either upload page or result page. cannot only verify one of
                // them since on fast device tab jump to result page really quick but on slow device
                // may stay on upload page for a really long time.
                return tab.getUrlString().equals(imageSuggestion.getUrl().getSpec())
                        || TemplateUrlServiceFactory.get()
                                   .isSearchResultsPageFromDefaultSearchProvider(
                                           tab.getUrlString());
            }
        });
    }

    @Test
    @SmallTest
    @Features.
    EnableFeatures({ChromeFeatureList.OMNIBOX_ENABLE_CLIPBOARD_PROVIDER_IMAGE_SUGGESTIONS})
    public void testImageSearch_OnlyTrustedIntentCanPost() throws InterruptedException, Exception {
        // Put an image into system clipboard.
        putAnImageIntoClipboard();

        // Start the Activity.
        final SearchActivity searchActivity = startSearchActivity();

        // Omnibox suggestions should appear now.
        final SearchActivityLocationBarLayout locationBar =
                (SearchActivityLocationBarLayout) searchActivity.findViewById(
                        R.id.search_location_bar);
        OmniboxTestUtils.waitForOmniboxSuggestions(locationBar, OMNIBOX_SHOW_TIMEOUT_MS);
        waitForSuggestionType(locationBar, OmniboxSuggestionType.CLIPBOARD_IMAGE);
        OmniboxSuggestionsDropdown suggestionsDropdown =
                AutocompleteCoordinatorTestUtils.getSuggestionsDropdown(
                        locationBar.getAutocompleteCoordinator());

        int imageSuggestionIndex = -1;
        // Find the index of the image clipboard suggestion.
        for (int i = 0; i < suggestionsDropdown.getItemCount(); ++i) {
            OmniboxSuggestion suggestion = AutocompleteCoordinatorTestUtils.getOmniboxSuggestionAt(
                    locationBar.getAutocompleteCoordinator(), i);
            if (suggestion != null
                    && suggestion.getType() == OmniboxSuggestionType.CLIPBOARD_IMAGE) {
                imageSuggestionIndex = i;
                break;
            }
        }
        Assert.assertNotEquals(
                "Cannot find the image clipboard Omnibox suggestion", -1, imageSuggestionIndex);

        OmniboxSuggestion imageSuggestion = AutocompleteCoordinatorTestUtils.getOmniboxSuggestionAt(
                locationBar.getAutocompleteCoordinator(), imageSuggestionIndex);

        Intent intent =
                new Intent(Intent.ACTION_VIEW, Uri.parse(imageSuggestion.getUrl().getSpec()));
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_NEW_DOCUMENT);
        intent.setClass(searchActivity, ChromeLauncherActivity.class);
        intent.putExtra(IntentHandler.EXTRA_POST_DATA_TYPE, imageSuggestion.getPostContentType());
        intent.putExtra(IntentHandler.EXTRA_POST_DATA, imageSuggestion.getPostData());

        final ChromeTabbedActivity cta =
                ActivityUtils.waitForActivity(InstrumentationRegistry.getInstrumentation(),
                        ChromeTabbedActivity.class, new Callable<Void>() {
                            @Override
                            public Void call() {
                                IntentUtils.safeStartActivity(searchActivity, intent);
                                return null;
                            }
                        });

        // Because no POST data, Google wont go to the result page.
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                Tab tab = cta.getActivityTab();
                return !TemplateUrlServiceFactory.get()
                                .isSearchResultsPageFromDefaultSearchProvider(tab.getUrlString());
            }
        });
    }

    private void putAnImageIntoClipboard() {
        mActivityTestRule.startMainActivityFromLauncher();
        ContentUriUtils.setFileProviderUtil(new FileProviderHelper());
        Bitmap bitmap =
                Bitmap.createBitmap(/* width = */ 10, /* height = */ 10, Bitmap.Config.ARGB_8888);
        ByteArrayOutputStream baos = new ByteArrayOutputStream();
        bitmap.compress(Bitmap.CompressFormat.PNG, /*quality = (0-100) */ 100, baos);
        byte[] mTestImageData = baos.toByteArray();
        Clipboard.getInstance().setImageFileProvider(new ClipboardImageFileProvider());
        Clipboard.getInstance().setImage(mTestImageData, TEST_PNG_IMAGE_FILE_EXTENSION);

        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return Clipboard.getInstance().getImage() != null;
            }
        });
    }

    private void clickSuggestionAt(OmniboxSuggestionsDropdown suggestionsDropdown, int index)
            throws InterruptedException {
        // Wait a bit since the button may not able to click.
        ViewGroup viewGroup = suggestionsDropdown.getViewGroup();
        BaseSuggestionView baseSuggestionView = (BaseSuggestionView) viewGroup.getChildAt(index);
        TestTouchUtils.performClickOnMainSync(InstrumentationRegistry.getInstrumentation(),
                baseSuggestionView.getDecoratedSuggestionView());
    }

    private SearchActivity startSearchActivity() {
        return startSearchActivity(0, /*isVoiceSearch=*/false);
    }

    private SearchActivity startSearchActivity(int expectedCallCount, boolean isVoiceSearch) {
        final Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();
        ActivityMonitor searchMonitor =
                new ActivityMonitor(SearchActivity.class.getName(), null, false);
        instrumentation.addMonitor(searchMonitor);

        // The SearchActivity shouldn't have started yet.
        Assert.assertEquals(expectedCallCount,
                mTestDelegate.shouldDelayNativeInitializationCallback.getCallCount());
        Assert.assertEquals(expectedCallCount,
                mTestDelegate.showSearchEngineDialogIfNeededCallback.getCallCount());
        Assert.assertEquals(expectedCallCount,
                mTestDelegate.onFinishDeferredInitializationCallback.getCallCount());

        // Fire the Intent to start up the SearchActivity.
        Intent intent = new Intent();
        SearchWidgetProvider.startSearchActivity(intent, isVoiceSearch);
        Activity searchActivity = instrumentation.waitForMonitorWithTimeout(
                searchMonitor, CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL);
        Assert.assertNotNull("Activity didn't start", searchActivity);
        Assert.assertTrue("Wrong activity started", searchActivity instanceof SearchActivity);
        instrumentation.removeMonitor(searchMonitor);
        return (SearchActivity) searchActivity;
    }

    private void waitForChromeTabbedActivityToStart(Callable<Void> trigger, String expectedUrl)
            throws Exception {
        final ChromeTabbedActivity cta = ActivityUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), ChromeTabbedActivity.class, trigger);

        CriteriaHelper.pollUiThread(Criteria.equals(expectedUrl, new Callable<String>() {
            @Override
            public String call() {
                Tab tab = cta.getActivityTab();
                if (tab == null) return null;

                return tab.getUrlString();
            }
        }));
    }

    private void waitForSuggestionType(final SearchActivityLocationBarLayout locationBar,
            final @OmniboxSuggestionType int type) {
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                OmniboxSuggestionsDropdown suggestionsDropdown =
                        AutocompleteCoordinatorTestUtils.getSuggestionsDropdown(
                                locationBar.getAutocompleteCoordinator());
                if (suggestionsDropdown == null) return false;

                for (int i = 0; i < suggestionsDropdown.getItemCount(); i++) {
                    OmniboxSuggestion suggestion =
                            AutocompleteCoordinatorTestUtils.getOmniboxSuggestionAt(
                                    locationBar.getAutocompleteCoordinator(), i);
                    if (suggestion != null && suggestion.getType() == type) {
                        return true;
                    }
                }
                return false;
            }
        });
    }

    @SuppressLint("SetTextI18n")
    private void setUrlBarText(final Activity activity, final String url) {
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                UrlBar urlBar = (UrlBar) activity.findViewById(R.id.url_bar);
                if (urlBar.isFocusable() && urlBar.hasFocus()) return true;
                urlBar.requestFocus();
                return false;
            }
        });
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            UrlBar urlBar = (UrlBar) activity.findViewById(R.id.url_bar);
            urlBar.setText(url);
        });
    }
}
