// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;
import android.os.Handler;
import android.os.SystemClock;
import android.text.TextUtils;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.Px;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ActivityTabProvider.ActivityTabTabObserver;
import org.chromium.chrome.browser.GlobalDiscardableReferencePool;
import org.chromium.chrome.browser.compositor.layouts.EmptyOverviewModeObserver;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.image_fetcher.ImageFetcher;
import org.chromium.chrome.browser.image_fetcher.ImageFetcherConfig;
import org.chromium.chrome.browser.image_fetcher.ImageFetcherFactory;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.StartStopWithNativeObserver;
import org.chromium.chrome.browser.omnibox.OmniboxSuggestionType;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController.OnSuggestionsReceivedListener;
import org.chromium.chrome.browser.omnibox.suggestions.answer.AnswerSuggestionProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.basic.BasicSuggestionProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionHost;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionViewDelegate;
import org.chromium.chrome.browser.omnibox.suggestions.clipboard.ClipboardSuggestionProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.editurl.EditUrlSuggestionProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.entity.EntitySuggestionProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.header.HeaderProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.tail.TailSuggestionProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.tiles.TileSuggestionProcessor;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.query_tiles.QueryTileUtils;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.ToolbarDataProvider;
import org.chromium.chrome.browser.ui.favicon.LargeIconBridge;
import org.chromium.components.browser_ui.util.ConversionUtils;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.query_tiles.QueryTile;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;

/**
 * Handles updating the model state for the currently visible omnibox suggestions.
 */
class AutocompleteMediator implements OnSuggestionsReceivedListener, StartStopWithNativeObserver,
                                      OmniboxSuggestionsDropdown.Observer, SuggestionHost {
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    static class DropdownItemViewInfo extends MVCListAdapter.ListItem {
        /** Processor managing the item. */
        public final DropdownItemProcessor processor;
        /** Group ID this ViewInfo belongs to. */
        public final int groupId;

        public DropdownItemViewInfo(
                DropdownItemProcessor processor, PropertyModel model, int groupId) {
            super(processor.getViewTypeId(), model);
            this.processor = processor;
            this.groupId = groupId;
        }

        /**
         * Initialize model for the encompassed suggestion.
         *
         * @param layoutDirection View layout direction (LTR or RTL).
         * @param useDarkColors Whether suggestions should be rendered using incognito or night mode
         *         colors.
         */
        void initializeModel(int layoutDirection, boolean useDarkColors) {
            model.set(SuggestionCommonProperties.LAYOUT_DIRECTION, layoutDirection);
            model.set(SuggestionCommonProperties.USE_DARK_COLORS, useDarkColors);
        }

        @Override
        public String toString() {
            return "DropdownItemViewInfo(group=" + groupId + ", type=" + type + ")";
        }
    }

    private static final String TAG = "Autocomplete";
    private static final int SUGGESTION_NOT_FOUND = -1;
    private static final int MINIMUM_NUMBER_OF_SUGGESTIONS_TO_SHOW = 5;

    private static final int MAX_IMAGE_CACHE_SIZE = 500 * ConversionUtils.BYTES_PER_KILOBYTE;

    // Delay triggering the omnibox results upon key press to allow the location bar to repaint
    // with the new characters.
    private static final long OMNIBOX_SUGGESTION_START_DELAY_MS = 30;
    private static final int OMNIBOX_HISTOGRAMS_MAX_SUGGESTIONS = 10;

    private final Context mContext;
    private final AutocompleteDelegate mDelegate;
    private final UrlBarEditingTextStateProvider mUrlBarEditingTextProvider;
    private final PropertyModel mListPropertyModel;
    private final List<Runnable> mDeferredNativeRunnables = new ArrayList<Runnable>();
    private final Handler mHandler;
    // TODO(crbug.com/982818): make EditUrlProcessor behave like all other processors and register
    // it in the mSuggestionProcessors list. The processor currently cannot be combined with
    // other processors because of its unique requirements.
    private @Nullable EditUrlSuggestionProcessor mEditUrlProcessor;
    private final TileSuggestionProcessor mTileSuggestionProcessor;
    private HeaderProcessor mHeaderProcessor;
    private final List<SuggestionProcessor> mSuggestionProcessors;
    private final List<DropdownItemViewInfo> mViewInfoList;
    private AutocompleteResult mAutocompleteResult;

    private ToolbarDataProvider mDataProvider;
    private OverviewModeBehavior mOverviewModeBehavior;
    private OverviewModeBehavior.OverviewModeObserver mOverviewModeObserver;

    private boolean mNativeInitialized;
    private AutocompleteController mAutocomplete;
    private long mUrlFocusTime;
    private boolean mEnableAdaptiveSuggestionsCount;
    @Px
    private int mMaximumSuggestionsListHeight;
    private boolean mEnableDeferredKeyboardPopup;
    private boolean mPendingKeyboardShowDecision;

    @IntDef({SuggestionVisibilityState.DISALLOWED, SuggestionVisibilityState.PENDING_ALLOW,
            SuggestionVisibilityState.ALLOWED})
    @Retention(RetentionPolicy.SOURCE)
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    @interface SuggestionVisibilityState {
        int DISALLOWED = 0;
        int PENDING_ALLOW = 1;
        int ALLOWED = 2;
    }
    @SuggestionVisibilityState
    private int mSuggestionVisibilityState;

    // The timestamp (using SystemClock.elapsedRealtime()) at the point when the user started
    // modifying the omnibox with new input.
    private long mNewOmniboxEditSessionTimestamp = -1;
    // Set to true when the user has started typing new input in the omnibox, set to false
    // when the omnibox loses focus or becomes empty.
    private boolean mHasStartedNewOmniboxEditSession;

    /**
     * The text shown in the URL bar (user text + inline autocomplete) after the most recent set of
     * omnibox suggestions was received. When the user presses enter in the omnibox, this value is
     * compared to the URL bar text to determine whether the first suggestion is still valid.
     */
    private String mUrlTextAfterSuggestionsReceived;

    private Runnable mRequestSuggestions;
    private DeferredOnSelectionRunnable mDeferredOnSelection;

    private boolean mShowCachedZeroSuggestResults;
    private boolean mShouldPreventOmniboxAutocomplete;

    private long mLastActionUpTimestamp;
    private boolean mIgnoreOmniboxItemSelection = true;
    private boolean mUseDarkColors = true;
    private int mLayoutDirection;

    private WindowAndroid mWindowAndroid;
    private ActivityLifecycleDispatcher mLifecycleDispatcher;
    private ActivityTabTabObserver mTabObserver;

    private ImageFetcher mImageFetcher;
    private LargeIconBridge mIconBridge;

    public AutocompleteMediator(Context context, AutocompleteDelegate delegate,
            UrlBarEditingTextStateProvider textProvider,
            AutocompleteController autocompleteController,
            Callback<List<QueryTile>> queryTileSuggestionCallback, PropertyModel listPropertyModel,
            Handler handler) {
        mContext = context;
        mDelegate = delegate;
        mUrlBarEditingTextProvider = textProvider;
        mListPropertyModel = listPropertyModel;
        mAutocomplete = autocompleteController;
        mAutocomplete.setOnSuggestionsReceivedListener(this);
        mHandler = handler;
        mTileSuggestionProcessor =
                new TileSuggestionProcessor(mContext, queryTileSuggestionCallback);
        mHeaderProcessor = new HeaderProcessor(mContext, this, mDelegate);
        mSuggestionProcessors = new ArrayList<>();
        mViewInfoList = new ArrayList<>();
        mAutocompleteResult = new AutocompleteResult(null, null);

        mOverviewModeObserver = new EmptyOverviewModeObserver() {
            @Override
            public void onOverviewModeStartedShowing(boolean showToolbar) {
                if (mDataProvider.shouldShowLocationBarInOverviewMode()) {
                    AutocompleteControllerJni.get().prefetchZeroSuggestResults();
                }
            }
        };
    }

    /**
     * Initialize the Mediator with default set of suggestions processors.
     */
    void initDefaultProcessors() {
        final Supplier<ImageFetcher> imageFetcherSupplier = createImageFetcherSupplier();
        final Supplier<LargeIconBridge> iconBridgeSupplier = createIconBridgeSupplier();

        mEditUrlProcessor =
                new EditUrlSuggestionProcessor(mContext, this, mDelegate, iconBridgeSupplier);
        registerSuggestionProcessor(new AnswerSuggestionProcessor(
                mContext, this, mUrlBarEditingTextProvider, imageFetcherSupplier));
        registerSuggestionProcessor(
                new ClipboardSuggestionProcessor(mContext, this, iconBridgeSupplier));
        registerSuggestionProcessor(
                new EntitySuggestionProcessor(mContext, this, imageFetcherSupplier));
        registerSuggestionProcessor(new TailSuggestionProcessor(mContext, this));
        registerSuggestionProcessor(mTileSuggestionProcessor);
        registerSuggestionProcessor(new BasicSuggestionProcessor(
                mContext, this, mUrlBarEditingTextProvider, iconBridgeSupplier));
    }

    /**
     * Register new processor to process OmniboxSuggestions.
     * Processors will be tried in the same order as they were added.
     *
     * @param processor SuggestionProcessor that handles OmniboxSuggestions.
     */
    void registerSuggestionProcessor(SuggestionProcessor processor) {
        mSuggestionProcessors.add(processor);
    }

    public void destroy() {
        if (mTabObserver != null) {
            mTabObserver.destroy();
        }
        if (mImageFetcher != null) {
            mImageFetcher.destroy();
            mImageFetcher = null;
        }
        if (mOverviewModeBehavior != null) {
            mOverviewModeBehavior.removeOverviewModeObserver(mOverviewModeObserver);
            mOverviewModeBehavior = null;
        }
    }

    @Override
    public void onStartWithNative() {}

    @Override
    public void onStopWithNative() {
        recordSuggestionsShown();
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    void setSuggestionVisibilityState(@SuggestionVisibilityState int state) {
        mSuggestionVisibilityState = state;
    }

    private @SuggestionVisibilityState int getSuggestionVisibilityState() {
        return mSuggestionVisibilityState;
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    ModelList getSuggestionModelList() {
        return mListPropertyModel.get(SuggestionListProperties.SUGGESTION_MODELS);
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    List<DropdownItemViewInfo> getSuggestionViewInfoListForTest() {
        return mViewInfoList;
    }

    /**
     * Create a new supplier that returns ImageFetcher instances.
     * Consumers of this call:
     * - should never cache the returned object, since its lifecycle is bound to external
     *   objects, such as Profile,
     * - should always check for null ahead of using returned value. ImageFetcher may not be
     *   constructed if Profile is not yet initialized.
     *
     * @return Supplier returning ImageFetcher.
     */
    private Supplier<ImageFetcher> createImageFetcherSupplier() {
        return new Supplier<ImageFetcher>() {
            @Override
            public ImageFetcher get() {
                if (getCurrentProfile() == null) {
                    return null;
                }
                if (mImageFetcher == null) {
                    mImageFetcher = ImageFetcherFactory.createImageFetcher(
                            ImageFetcherConfig.IN_MEMORY_ONLY,
                            GlobalDiscardableReferencePool.getReferencePool(),
                            MAX_IMAGE_CACHE_SIZE);
                }
                return mImageFetcher;
            }
        };
    }

    /**
     * Create a new supplier that returns LargeIconBridge instances.
     * Consumers of this call:
     * - should never cache the returned object, since its lifecycle is bound to external
     *   objects, such as Profile,
     * - should always check for null ahead of using returned value. LargeIconBridge may not be
     *   constructed if Profile is not yet initialized.
     *
     * @return Supplier returning LargeIconBridge.
     */
    private Supplier<LargeIconBridge> createIconBridgeSupplier() {
        return new Supplier<LargeIconBridge>() {
            @Override
            public LargeIconBridge get() {
                if (getCurrentProfile() == null) {
                    return null;
                }
                if (mIconBridge == null) {
                    mIconBridge = new LargeIconBridge(getCurrentProfile());
                }
                return mIconBridge;
            }
        };
    }

    private Profile getCurrentProfile() {
        return mDataProvider != null ? mDataProvider.getProfile() : null;
    }

    /**
     * Check if the suggestion is created from clipboard.
     *
     * @param suggestion The OmniboxSuggestion to check.
     * @return Whether or not the suggestion is from clipboard.
     */
    private boolean isSuggestionFromClipboard(OmniboxSuggestion suggestion) {
        return suggestion.getType() == OmniboxSuggestionType.CLIPBOARD_URL
                || suggestion.getType() == OmniboxSuggestionType.CLIPBOARD_TEXT
                || suggestion.getType() == OmniboxSuggestionType.CLIPBOARD_IMAGE;
    }

    /**
     * Record histograms for presented suggestions.
     */
    private void recordSuggestionsShown() {
        int richEntitiesCount = 0;
        ModelList currentModels = getSuggestionModelList();
        for (int i = 0; i < currentModels.size(); i++) {
            DropdownItemViewInfo info = (DropdownItemViewInfo) currentModels.get(i);
            info.processor.recordItemPresented(info.model);

            if (info.type == OmniboxSuggestionUiType.ENTITY_SUGGESTION) {
                richEntitiesCount++;
            }
        }

        // Note: valid range for histograms must start with (at least) 1. This does not prevent us
        // from reporting 0 as a count though - values lower than 'min' fall in the 'underflow'
        // bucket, while values larger than 'max' will be reported in 'overflow' bucket.
        RecordHistogram.recordLinearCountHistogram("Omnibox.RichEntityShown", richEntitiesCount, 1,
                OMNIBOX_HISTOGRAMS_MAX_SUGGESTIONS, OMNIBOX_HISTOGRAMS_MAX_SUGGESTIONS + 1);
    }

    /**
     * @return The number of current autocomplete suggestions.
     */
    public int getSuggestionCount() {
        return mAutocompleteResult.getSuggestionsList().size();
    }

    /**
     * Retrieve the omnibox suggestion at the specified index.  The index represents the ordering
     * in the underlying model.  The index does not represent visibility due to the current scroll
     * position of the list.
     *
     * @param index The index of the suggestion to fetch.
     * @return The suggestion at the given index.
     */
    public OmniboxSuggestion getSuggestionAt(int index) {
        return mAutocompleteResult.getSuggestionsList().get(index);
    }

    /**
     * Sets the data provider for the toolbar.
     */
    void setToolbarDataProvider(ToolbarDataProvider provider) {
        mDataProvider = provider;
    }

    /**
     * @param overviewModeBehavior A means of accessing the current OverviewModeState and a way to
     *         listen to state changes.
     */
    public void setOverviewModeBehavior(OverviewModeBehavior overviewModeBehavior) {
        assert mOverviewModeBehavior == null;

        mOverviewModeBehavior = overviewModeBehavior;
        mOverviewModeBehavior.addOverviewModeObserver(mOverviewModeObserver);
    }

    /** Set the WindowAndroid instance associated with the containing Activity. */
    void setWindowAndroid(WindowAndroid window) {
        if (mLifecycleDispatcher != null) {
            mLifecycleDispatcher.unregister(this);
        }

        mWindowAndroid = window;
        if (window != null && window.getActivity().get() != null
                && window.getActivity().get() instanceof AsyncInitializationActivity) {
            mLifecycleDispatcher =
                    ((AsyncInitializationActivity) mWindowAndroid.getActivity().get())
                            .getLifecycleDispatcher();
        }

        if (mLifecycleDispatcher != null) {
            mLifecycleDispatcher.register(this);
        }
    }

    /**
     * Sets the layout direction to be used for any new suggestion views.
     * @see View#setLayoutDirection(int)
     */
    void setLayoutDirection(int layoutDirection) {
        if (mLayoutDirection == layoutDirection) return;
        mLayoutDirection = layoutDirection;
        ModelList currentModels = getSuggestionModelList();
        for (int i = 0; i < currentModels.size(); i++) {
            PropertyModel model = currentModels.get(i).model;
            model.set(SuggestionCommonProperties.LAYOUT_DIRECTION, layoutDirection);
        }
    }

    /**
     * Specifies the visual state to be used by the suggestions.
     * @param useDarkColors Whether dark colors should be used for fonts and icons.
     * @param isIncognito Whether the UI is for incognito mode or not.
     */
    void updateVisualsForState(boolean useDarkColors, boolean isIncognito) {
        mUseDarkColors = useDarkColors;
        mListPropertyModel.set(SuggestionListProperties.IS_INCOGNITO, isIncognito);
        ModelList currentModels = getSuggestionModelList();
        for (int i = 0; i < currentModels.size(); i++) {
            PropertyModel model = currentModels.get(i).model;
            model.set(SuggestionCommonProperties.USE_DARK_COLORS, useDarkColors);
        }
    }

    /**
     * Sets to show cached zero suggest results. This will start both caching zero suggest results
     * in shared preferences and also attempt to show them when appropriate without needing native
     * initialization.
     * @param showCachedZeroSuggestResults Whether cached zero suggest should be shown.
     */
    void setShowCachedZeroSuggestResults(boolean showCachedZeroSuggestResults) {
        mShowCachedZeroSuggestResults = showCachedZeroSuggestResults;
        if (mShowCachedZeroSuggestResults) mAutocomplete.startCachedZeroSuggest();
    }

    /** Notify the mediator that a item selection is pending and should be accepted. */
    void allowPendingItemSelection() {
        mIgnoreOmniboxItemSelection = false;
    }

    /**
     * Signals that native initialization has completed.
     */
    void onNativeInitialized() {
        mNativeInitialized = true;

        mEnableAdaptiveSuggestionsCount =
                ChromeFeatureList.isEnabled(ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT);
        mEnableDeferredKeyboardPopup =
                ChromeFeatureList.isEnabled(ChromeFeatureList.OMNIBOX_DEFERRED_KEYBOARD_POPUP);

        for (Runnable deferredRunnable : mDeferredNativeRunnables) {
            mHandler.post(deferredRunnable);
        }
        mDeferredNativeRunnables.clear();

        for (SuggestionProcessor processor : mSuggestionProcessors) {
            processor.onNativeInitialized();
        }
        if (mEditUrlProcessor != null) mEditUrlProcessor.onNativeInitialized();
    }

    /**
     * @param provider A means of accessing the activity tab.
     */
    void setActivityTabProvider(ActivityTabProvider provider) {
        if (mEditUrlProcessor != null) mEditUrlProcessor.setActivityTabProvider(provider);

        if (mTabObserver != null) {
            mTabObserver.destroy();
            mTabObserver = null;
        }

        if (provider != null) {
            mTabObserver = new ActivityTabTabObserver(provider) {
                @Override
                public void onPageLoadFinished(Tab tab, String url) {
                    maybeTriggerCacheRefresh(url);
                }

                @Override
                protected void onObservingDifferentTab(Tab tab) {
                    if (tab == null) return;
                    maybeTriggerCacheRefresh(tab.getUrlString());
                }

                /**
                 * Trigger ZeroSuggest cache refresh in case user is accessing a new tab page.
                 * Avoid issuing multiple concurrent server requests for the same event to reduce
                 * server pressure.
                 */
                private void maybeTriggerCacheRefresh(String url) {
                    if (url == null) return;
                    if (!UrlConstants.NTP_URL.equals(url)) return;
                    AutocompleteControllerJni.get().prefetchZeroSuggestResults();
                }
            };
        }
    }

    void setShareDelegateSupplier(Supplier<ShareDelegate> shareDelegateSupplier) {
        mEditUrlProcessor.setShareDelegateSupplier(shareDelegateSupplier);
    }

    /** @see org.chromium.chrome.browser.omnibox.UrlFocusChangeListener#onUrlFocusChange(boolean) */
    void onUrlFocusChange(boolean hasFocus) {
        if (hasFocus) {
            mUrlFocusTime = System.currentTimeMillis();
            setSuggestionVisibilityState(SuggestionVisibilityState.PENDING_ALLOW);

            signalPendingKeyboardShowDecision();
            // For cases where we know the feature is disabled - or those where Omnibox is running
            // without native code loaded - make sure we present the keyboard immediately.
            if (!mEnableDeferredKeyboardPopup) {
                resolvePendingKeyboardShowDecision();
            }

            if (mNativeInitialized) {
                startZeroSuggest();
            } else {
                mDeferredNativeRunnables.add(() -> {
                    if (TextUtils.isEmpty(mUrlBarEditingTextProvider.getTextWithAutocomplete())) {
                        startZeroSuggest();
                    }
                });
            }
        } else {
            if (mNativeInitialized) recordSuggestionsShown();

            setSuggestionVisibilityState(SuggestionVisibilityState.DISALLOWED);
            mHasStartedNewOmniboxEditSession = false;
            mNewOmniboxEditSessionTimestamp = -1;
            // Prevent any upcoming omnibox suggestions from showing once a URL is loaded (and as
            // a consequence the omnibox is unfocused).
            hideSuggestions();

            if (mImageFetcher != null) mImageFetcher.clear();
        }

        if (mEditUrlProcessor != null) mEditUrlProcessor.onUrlFocusChange(hasFocus);
        for (SuggestionProcessor processor : mSuggestionProcessors) {
            processor.onUrlFocusChange(hasFocus);
        }
    }

    /**
     * @see
     * org.chromium.chrome.browser.omnibox.UrlFocusChangeListener#onUrlAnimationFinished(boolean)
     */
    void onUrlAnimationFinished(boolean hasFocus) {
        setSuggestionVisibilityState(hasFocus ? SuggestionVisibilityState.ALLOWED
                                              : SuggestionVisibilityState.DISALLOWED);
        updateOmniboxSuggestionsVisibility();
    }

    /**
     * Updates the profile used for generating autocomplete suggestions.
     * @param profile The profile to be used.
     */
    void setAutocompleteProfile(Profile profile) {
        mAutocomplete.setProfile(profile);
        mIconBridge = null;
    }

    /**
     * Whether omnibox autocomplete should currently be prevented from generating suggestions.
     */
    void setShouldPreventOmniboxAutocomplete(boolean prevent) {
        mShouldPreventOmniboxAutocomplete = prevent;
    }

    /**
     * @see AutocompleteController#onVoiceResults(List)
     */
    void onVoiceResults(@Nullable List<VoiceRecognitionHandler.VoiceResult> results) {
        mAutocomplete.onVoiceResults(results);
    }

    /**
     * @return The current native pointer to the autocomplete results.
     */
    // TODO(tedchoc): Figure out how to remove this.
    long getCurrentNativeAutocompleteResult() {
        return mAutocomplete.getCurrentNativeAutocompleteResult();
    }

    @Override
    public SuggestionViewDelegate createSuggestionViewDelegate(
            OmniboxSuggestion suggestion, int position) {
        return new SuggestionViewDelegate() {
            @Override
            public void onSetUrlToSuggestion() {
                if (mIgnoreOmniboxItemSelection) return;
                mIgnoreOmniboxItemSelection = true;
                AutocompleteMediator.this.onSetUrlToSuggestion(suggestion);
            }

            @Override
            public void onSelection() {
                AutocompleteMediator.this.onSelection(suggestion, position);
            }

            @Override
            public void onLongPress() {
                AutocompleteMediator.this.onLongPress(suggestion, position);
            }

            @Override
            public void onGestureUp(long timestamp) {
                mLastActionUpTimestamp = timestamp;
            }

            @Override
            public void onGestureDown() {
                stopAutocomplete(false);
            }
        };
    }

    /** Called when a query tile is selected by the user. */
    void onQueryTileSelected(QueryTile queryTile) {
        // For last level tile, start a search query, unless we want to let user have a chance to
        // edit the query.
        if (queryTile.children.isEmpty() && !QueryTileUtils.isQueryEditingEnabled()) {
            String url = TemplateUrlServiceFactory.get().getUrlForSearchQuery(
                    queryTile.queryText, queryTile.searchParams);
            mDelegate.loadUrl(url, PageTransition.LINK, mLastActionUpTimestamp);
            mDelegate.setKeyboardVisibility(false);
            return;
        }

        // If the tile has sub-tiles, start a new request to the backend to get the new set
        // of tiles. Also set the tile text in omnibox.
        stopAutocomplete(false);
        String refineText = TextUtils.concat(queryTile.queryText, " ").toString();
        mDelegate.setOmniboxEditingText(refineText);

        mNewOmniboxEditSessionTimestamp = SystemClock.elapsedRealtime();
        mHasStartedNewOmniboxEditSession = true;

        mAutocomplete.start(mDataProvider.getProfile(), mDataProvider.getCurrentUrl(),
                mDataProvider.getPageClassification(mDelegate.didFocusUrlFromFakebox()),
                mUrlBarEditingTextProvider.getTextWithoutAutocomplete(),
                mUrlBarEditingTextProvider.getSelectionStart(),
                !mUrlBarEditingTextProvider.shouldAutocomplete(), queryTile.id);
    }

    /**
     * Triggered when the user selects one of the omnibox suggestions to navigate to.
     * @param suggestion The OmniboxSuggestion which was selected.
     * @param position Position of the suggestion in the drop down view.
     */
    private void onSelection(OmniboxSuggestion suggestion, int position) {
        if (mShowCachedZeroSuggestResults && !mNativeInitialized) {
            mDeferredOnSelection = new DeferredOnSelectionRunnable(suggestion, position) {
                @Override
                public void run() {
                    onSelection(this.mSuggestion, this.mPosition);
                }
            };
            return;
        }

        loadUrlFromOmniboxMatch(position, suggestion, mLastActionUpTimestamp, true);
        mDelegate.setKeyboardVisibility(false);
    }

    /**
     * Triggered when the user selects to refine one of the omnibox suggestions.
     * @param suggestion The suggestion selected.
     */
    @Override
    public void onRefineSuggestion(OmniboxSuggestion suggestion) {
        stopAutocomplete(false);
        boolean isSearchSuggestion = suggestion.isSearchSuggestion();
        String refineText = suggestion.getFillIntoEdit();
        if (isSearchSuggestion) refineText = TextUtils.concat(refineText, " ").toString();

        mDelegate.setOmniboxEditingText(refineText);
        onTextChanged(mUrlBarEditingTextProvider.getTextWithoutAutocomplete(),
                mUrlBarEditingTextProvider.getTextWithAutocomplete());
        if (isSearchSuggestion) {
            RecordUserAction.record("MobileOmniboxRefineSuggestion.Search");
        } else {
            RecordUserAction.record("MobileOmniboxRefineSuggestion.Url");
        }
    }

    /**
     * Triggered when the user long presses the omnibox suggestion.
     * @param suggestion The suggestion selected.
     * @param position The position of the suggestion.
     */
    private void onLongPress(OmniboxSuggestion suggestion, int position) {
        RecordUserAction.record("MobileOmniboxDeleteGesture");
        if (!suggestion.isDeletable()) return;

        if (mWindowAndroid == null) return;
        Activity activity = mWindowAndroid.getActivity().get();
        if (activity == null || !(activity instanceof AsyncInitializationActivity)) return;
        ModalDialogManager manager =
                ((AsyncInitializationActivity) activity).getModalDialogManager();
        if (manager == null) {
            assert false : "No modal dialog manager registered for this activity.";
            return;
        }

        ModalDialogProperties.Controller dialogController = new ModalDialogProperties.Controller() {
            @Override
            public void onClick(PropertyModel model, int buttonType) {
                if (buttonType == ModalDialogProperties.ButtonType.POSITIVE) {
                    RecordUserAction.record("MobileOmniboxDeleteRequested");
                    mAutocomplete.deleteSuggestion(position, suggestion.hashCode());
                    manager.dismissDialog(model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                } else if (buttonType == ModalDialogProperties.ButtonType.NEGATIVE) {
                    manager.dismissDialog(model, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
                }
            }

            @Override
            public void onDismiss(PropertyModel model, int dismissalCause) {}
        };

        Resources resources = mContext.getResources();
        @StringRes
        int dialogMessageId = R.string.omnibox_confirm_delete;
        if (isSuggestionFromClipboard(suggestion)) {
            dialogMessageId = R.string.omnibox_confirm_delete_from_clipboard;
        }

        PropertyModel model =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, dialogController)
                        .with(ModalDialogProperties.TITLE, suggestion.getDisplayText())
                        .with(ModalDialogProperties.MESSAGE, resources, dialogMessageId)
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, resources, R.string.ok)
                        .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, resources,
                                R.string.cancel)
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                        .build();

        // Prevent updates to the shown omnibox suggestions list while the dialog is open.
        stopAutocomplete(false);
        manager.showDialog(model, ModalDialogManager.ModalDialogType.APP);
    }

    /**
     * Triggered when the user navigates to one of the suggestions without clicking on it.
     * @param suggestion The suggestion that was selected.
     */
    void onSetUrlToSuggestion(OmniboxSuggestion suggestion) {
        mDelegate.setOmniboxEditingText(suggestion.getFillIntoEdit());
    }

    /**
     * Updates the URL we will navigate to from suggestion, if needed. This will update the search
     * URL to be of the corpus type if query in the omnibox is displayed and update aqs= parameter
     * on regular web search URLs.
     *
     * @param suggestion The chosen omnibox suggestion.
     * @param selectedIndex The index of the chosen omnibox suggestion.
     * @param skipCheck Whether to skip an out of bounds check.
     * @return The url to navigate to.
     */
    private GURL updateSuggestionUrlIfNeeded(
            OmniboxSuggestion suggestion, int selectedIndex, boolean skipCheck) {
        // Only called once we have suggestions, and don't have a listener though which we can
        // receive suggestions until the native side is ready, so this is safe
        assert mNativeInitialized
            : "updateSuggestionUrlIfNeeded called before native initialization";

        if (suggestion.getType() == OmniboxSuggestionType.VOICE_SUGGEST
                || suggestion.getType() == OmniboxSuggestionType.TILE_SUGGESTION) {
            return suggestion.getUrl();
        }

        int verifiedIndex = SUGGESTION_NOT_FOUND;
        if (!skipCheck) {
            verifiedIndex = findSuggestionInAutocompleteResult(suggestion, selectedIndex);
        }

        // If we do not have the suggestion as part of our results, skip the URL update.
        if (verifiedIndex == SUGGESTION_NOT_FOUND) return suggestion.getUrl();

        // TODO(mariakhomenko): Ideally we want to update match destination URL with new aqs
        // for query in the omnibox and voice suggestions, but it's currently difficult to do.
        long elapsedTimeSinceInputChange = mNewOmniboxEditSessionTimestamp > 0
                ? (SystemClock.elapsedRealtime() - mNewOmniboxEditSessionTimestamp)
                : -1;
        GURL updatedUrl = mAutocomplete.updateMatchDestinationUrlWithQueryFormulationTime(
                verifiedIndex, suggestion.hashCode(), elapsedTimeSinceInputChange);

        return updatedUrl == null ? suggestion.getUrl() : updatedUrl;
    }

    /**
     * Check if the supplied suggestion is still in the current model and return its index.
     *
     * This call should be used to confirm that model has not been changed ahead of an event being
     * called by all the methods that are dispatched rather than called directly.
     *
     * @param suggestion Suggestion to look for.
     * @param index Last known position of the suggestion.
     * @return Current index of the supplied suggestion, or SUGGESTION_NOT_FOUND if it is no longer
     *         part of the model.
     */
    @SuppressWarnings("ReferenceEquality")
    private int findSuggestionInAutocompleteResult(OmniboxSuggestion suggestion, int position) {
        if (getSuggestionCount() > position && getSuggestionAt(position) == suggestion) {
            return position;
        }

        // Underlying omnibox results may have changed since the selection was made,
        // find the suggestion item, if possible.
        for (int index = 0; index < getSuggestionCount(); index++) {
            if (suggestion.equals(getSuggestionAt(index))) {
                return index;
            }
        }

        return SUGGESTION_NOT_FOUND;
    }

    /**
     * Notifies the autocomplete system that the text has changed that drives autocomplete and the
     * autocomplete suggestions should be updated.
     */
    public void onTextChanged(String textWithoutAutocomplete, String textWithAutocomplete) {
        // crbug.com/764749
        Log.w(TAG, "onTextChangedForAutocomplete");

        if (mShouldPreventOmniboxAutocomplete) return;

        mIgnoreOmniboxItemSelection = true;
        cancelPendingAutocompleteStart();

        if (!mHasStartedNewOmniboxEditSession && mNativeInitialized) {
            mAutocomplete.resetSession();
            mNewOmniboxEditSessionTimestamp = SystemClock.elapsedRealtime();
            mHasStartedNewOmniboxEditSession = true;
        }

        stopAutocomplete(false);
        if (TextUtils.isEmpty(textWithoutAutocomplete)) {
            // crbug.com/764749
            Log.w(TAG, "onTextChangedForAutocomplete: url is empty");
            hideSuggestions();
            startZeroSuggest();
        } else {
            assert mRequestSuggestions == null : "Multiple omnibox requests in flight.";
            mRequestSuggestions = () -> {
                boolean preventAutocomplete = !mUrlBarEditingTextProvider.shouldAutocomplete();
                mRequestSuggestions = null;

                // There may be no tabs when searching form omnibox in overview mode. In that case,
                // ToolbarDataProvider.getCurrentUrl() returns NTP url.
                if (!mDataProvider.hasTab() && !mDataProvider.isInOverviewAndShowingOmnibox()) {
                    // crbug.com/764749
                    Log.w(TAG, "onTextChangedForAutocomplete: no tab");
                    return;
                }

                Profile profile = mDataProvider.getProfile();
                int cursorPosition = -1;
                if (mUrlBarEditingTextProvider.getSelectionStart()
                        == mUrlBarEditingTextProvider.getSelectionEnd()) {
                    // Conveniently, if there is no selection, those two functions return -1,
                    // exactly the same value needed to pass to start() to indicate no cursor
                    // position.  Hence, there's no need to check for -1 here explicitly.
                    cursorPosition = mUrlBarEditingTextProvider.getSelectionStart();
                }
                int pageClassification =
                        mDataProvider.getPageClassification(mDelegate.didFocusUrlFromFakebox());
                mAutocomplete.start(profile, mDataProvider.getCurrentUrl(), pageClassification,
                        textWithoutAutocomplete, cursorPosition, preventAutocomplete, null);
            };
            if (mNativeInitialized) {
                mHandler.postDelayed(mRequestSuggestions, OMNIBOX_SUGGESTION_START_DELAY_MS);
            } else {
                mDeferredNativeRunnables.add(mRequestSuggestions);
            }
        }

        mDelegate.onUrlTextChanged();
    }

    /**
     * @param suggestion The suggestion to be processed.
     * @return The appropriate suggestion processor for the provided suggestion.
     */
    private SuggestionProcessor getProcessorForSuggestion(
            OmniboxSuggestion suggestion, boolean isFirst) {
        if (isFirst && mEditUrlProcessor != null
                && mEditUrlProcessor.doesProcessSuggestion(suggestion)) {
            return mEditUrlProcessor;
        }

        for (SuggestionProcessor processor : mSuggestionProcessors) {
            if (processor.doesProcessSuggestion(suggestion)) return processor;
        }
        assert false : "No default handler for suggestions";
        return null;
    }

    /**
     * Set signal indicating that the AutocompleteMediator should issue request to show or hide
     * keyboard upon receiving next batch of Suggestions.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    void signalPendingKeyboardShowDecision() {
        mPendingKeyboardShowDecision = true;
    }

    /**
     * Issue request to show or hide keyboard after receiving fresh set of suggestions.
     * The request is only issued if it was previously signalled as 'pending'.
     */
    private void resolvePendingKeyboardShowDecision() {
        if (!mPendingKeyboardShowDecision) return;
        mPendingKeyboardShowDecision = false;
        mDelegate.setKeyboardVisibility(shouldShowSoftKeyboard());
    }

    /**
     * @return True if soft keyboard should be shown.
     */
    private boolean shouldShowSoftKeyboard() {
        return !mEnableDeferredKeyboardPopup
                || mViewInfoList.size() <= MINIMUM_NUMBER_OF_SUGGESTIONS_TO_SHOW;
    }

    @Override
    public void onSuggestionsReceived(
            AutocompleteResult autocompleteResult, String inlineAutocompleteText) {
        if (mShouldPreventOmniboxAutocomplete
                || getSuggestionVisibilityState() == SuggestionVisibilityState.DISALLOWED) {
            resolvePendingKeyboardShowDecision();
            return;
        }

        // This is a callback from a listener that is set up by onNativeLibraryReady,
        // so can only be called once the native side is set up unless we are showing
        // cached java-only suggestions.
        assert mNativeInitialized
                || mShowCachedZeroSuggestResults
            : "Native suggestions received before native side intialialized";

        final List<OmniboxSuggestion> newSuggestions = autocompleteResult.getSuggestionsList();
        if (mDeferredOnSelection != null) {
            mDeferredOnSelection.setShouldLog(newSuggestions.size() > mDeferredOnSelection.mPosition
                    && mDeferredOnSelection.mSuggestion.equals(
                            newSuggestions.get(mDeferredOnSelection.mPosition)));
            mDeferredOnSelection.run();
            mDeferredOnSelection = null;
        }
        String userText = mUrlBarEditingTextProvider.getTextWithoutAutocomplete();
        mUrlTextAfterSuggestionsReceived = userText + inlineAutocompleteText;

        if (setNewSuggestions(autocompleteResult)) {
            // Reset all processors and clear existing suggestions if we received a new suggestions
            // list.
            for (SuggestionProcessor processor : mSuggestionProcessors) {
                processor.onSuggestionsReceived();
            }
            mDelegate.onSuggestionsChanged(inlineAutocompleteText);
            updateSuggestionsList(mMaximumSuggestionsListHeight);
        }
        resolvePendingKeyboardShowDecision();
    }

    @Override
    public void setGroupVisibility(int groupId, boolean state) {
        if (state) {
            insertSuggestionsForGroup(groupId);
        } else {
            removeSuggestionsForGroup(groupId);
        }
    }

    /**
     * @return True, if view info is a header for the specific group of suggestions.
     */
    private boolean isGroupHeaderWithId(DropdownItemViewInfo info, int groupId) {
        return (info.type == OmniboxSuggestionUiType.HEADER && info.groupId == groupId);
    }

    /**
     * Remove all suggestions that belong to specific group.
     *
     * @param groupId Group ID of suggestions that should be removed.
     */
    private void removeSuggestionsForGroup(int groupId) {
        final ModelList modelList = getSuggestionModelList();
        int index;
        int count = 0;

        for (index = modelList.size() - 1; index >= 0; index--) {
            DropdownItemViewInfo viewInfo = (DropdownItemViewInfo) modelList.get(index);
            if (isGroupHeaderWithId(viewInfo, groupId)) {
                break;
            } else if (viewInfo.groupId == groupId) {
                count++;
            } else if (count > 0 && viewInfo.groupId != groupId) {
                break;
            }
        }
        if (count > 0) {
            // Skip group header when dropping items.
            modelList.removeRange(index + 1, count);
        }
    }

    /**
     * Insert all suggestions that belong to specific group.
     *
     * @param groupId Group ID of suggestions that should be removed.
     */
    private void insertSuggestionsForGroup(int groupId) {
        final ModelList offeredViewInfoList = getSuggestionModelList();
        int insertPosition = 0;

        // Search for the insert position.
        // Iterate through all *available* view infos until we find the first element that we
        // should insert. To determine the insertion point we skip past all *displayed* view
        // infos that were also preceding elements that we want to insert.
        for (; insertPosition < offeredViewInfoList.size(); insertPosition++) {
            final DropdownItemViewInfo viewInfo =
                    (DropdownItemViewInfo) offeredViewInfoList.get(insertPosition);
            // Insert suggestions directly below their header.
            if (isGroupHeaderWithId(viewInfo, groupId)) break;
        }

        // Check if reached the end of the list.
        if (insertPosition == offeredViewInfoList.size()) return;

        // insertPosition points to header - advance the index and see if we already have
        // elements belonging to that group on the list.
        insertPosition++;
        if (insertPosition < offeredViewInfoList.size()
                && ((DropdownItemViewInfo) offeredViewInfoList.get(insertPosition)).groupId
                        == groupId) {
            return;
        }

        // Find elements to insert.
        int firstElementIndex = -1;
        int count = 0;
        for (int index = 0; index < mViewInfoList.size(); index++) {
            final DropdownItemViewInfo viewInfo = mViewInfoList.get(index);
            if (isGroupHeaderWithId(viewInfo, groupId)) {
                firstElementIndex = index + 1;
            } else if (viewInfo.groupId == groupId) {
                count++;
            } else if (count > 0 && viewInfo.groupId != groupId) {
                break;
            }
        }

        if (count != 0 && firstElementIndex != -1) {
            offeredViewInfoList.addAll(
                    mViewInfoList.subList(firstElementIndex, firstElementIndex + count),
                    insertPosition);
        }
    }

    /**
     * Process the supplied SuggestionList to internal representation.
     *
     * TODO(https://crbug.com/982818): identify suggestions that have simply changed places and
     * re-use them.
     *
     * @param autocompleteResult New autocomplete details.
     * @return true, if newly supplied list was different from the previously supplied and cached
     *         list.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    boolean setNewSuggestions(AutocompleteResult autocompleteResult) {
        if (mAutocompleteResult.equals(autocompleteResult)) return false;
        mAutocompleteResult = autocompleteResult;

        final List<OmniboxSuggestion> newSuggestions = mAutocompleteResult.getSuggestionsList();
        final int newSuggestionsCount = newSuggestions.size();

        mViewInfoList.clear();
        int currentGroup = OmniboxSuggestion.INVALID_GROUP;

        for (int index = 0; index < newSuggestionsCount; index++) {
            final OmniboxSuggestion suggestion = newSuggestions.get(index);

            if (suggestion.getGroupId() != currentGroup) {
                currentGroup = suggestion.getGroupId();
                final PropertyModel model = mHeaderProcessor.createModel();
                mHeaderProcessor.populateModel(model, currentGroup,
                        mAutocompleteResult.getGroupHeaders().get(currentGroup));
                mViewInfoList.add(new DropdownItemViewInfo(mHeaderProcessor, model, currentGroup));
            }

            final SuggestionProcessor processor = getProcessorForSuggestion(suggestion, index == 0);
            final PropertyModel model = processor.createModel();
            processor.populateModel(suggestion, model, index);
            mViewInfoList.add(new DropdownItemViewInfo(processor, model, currentGroup));
        }

        return true;
    }

    @NonNull
    AutocompleteResult getAutocompleteResult() {
        return mAutocompleteResult;
    }

    /**
     * Refresh list of presented suggestions.
     *
     * @param maximumListHeightPx Maximum height of the Suggestions list that guarantees 100%
     *         content visibility.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    void updateSuggestionsList(int maximumListHeightPx) {
        if (mViewInfoList.isEmpty()) {
            hideSuggestions();
            return;
        }

        final List<MVCListAdapter.ListItem> newSuggestionViewInfos =
                new ArrayList<>(mViewInfoList.size());
        final ModelList prepopulatedSuggestions = getSuggestionModelList();

        final int numSuggestionsToShow = mViewInfoList.size();
        int totalSuggestionsHeight = 0;

        for (int suggestionIndex = 0; suggestionIndex < numSuggestionsToShow; suggestionIndex++) {
            final DropdownItemViewInfo viewInfo =
                    (DropdownItemViewInfo) mViewInfoList.get(suggestionIndex);

            totalSuggestionsHeight += viewInfo.processor.getMinimumViewHeight();
            if (mEnableAdaptiveSuggestionsCount
                    && suggestionIndex >= MINIMUM_NUMBER_OF_SUGGESTIONS_TO_SHOW
                    && totalSuggestionsHeight > maximumListHeightPx) {
                break;
            }

            viewInfo.initializeModel(mLayoutDirection, mUseDarkColors);
            newSuggestionViewInfos.add(viewInfo);
        }

        prepopulatedSuggestions.set(newSuggestionViewInfos);

        if (mListPropertyModel.get(SuggestionListProperties.VISIBLE)
                && newSuggestionViewInfos.size() == 0) {
            hideSuggestions();
        }
        updateOmniboxSuggestionsVisibility();
    }

    /**
     * Load the url corresponding to the typed omnibox text.
     * @param eventTime The timestamp the load was triggered by the user.
     */
    void loadTypedOmniboxText(long eventTime) {
        mDelegate.setKeyboardVisibility(false);

        final String urlText = mUrlBarEditingTextProvider.getTextWithAutocomplete();
        if (mNativeInitialized) {
            findMatchAndLoadUrl(urlText, eventTime);
        } else {
            mDeferredNativeRunnables.add(() -> findMatchAndLoadUrl(urlText, eventTime));
        }
    }

    private void findMatchAndLoadUrl(String urlText, long inputStart) {
        OmniboxSuggestion suggestionMatch;
        boolean inSuggestionList = true;

        if (getSuggestionCount() > 0
                && urlText.trim().equals(mUrlTextAfterSuggestionsReceived.trim())) {
            // Common case: the user typed something, received suggestions, then pressed enter.
            suggestionMatch = getSuggestionAt(0);
        } else {
            // Less common case: there are no valid omnibox suggestions. This can happen if the
            // user tapped the URL bar to dismiss the suggestions, then pressed enter. This can
            // also happen if the user presses enter before any suggestions have been received
            // from the autocomplete controller.
            suggestionMatch = mAutocomplete.classify(urlText, mDelegate.didFocusUrlFromFakebox());
            // Classify matches don't propagate to java, so skip the OOB check.
            inSuggestionList = false;

            // If urlText couldn't be classified, bail.
            if (suggestionMatch == null) return;
        }

        loadUrlFromOmniboxMatch(0, suggestionMatch, inputStart, inSuggestionList);
    }

    /**
     * Loads the specified omnibox suggestion.
     *
     * @param matchPosition The position of the selected omnibox suggestion.
     * @param suggestion The suggestion selected.
     * @param inputStart The timestamp the input was started.
     * @param inVisibleSuggestionList Whether the suggestion is in the visible suggestion list.
     */
    private void loadUrlFromOmniboxMatch(int matchPosition, OmniboxSuggestion suggestion,
            long inputStart, boolean inVisibleSuggestionList) {
        final long activationTime = System.currentTimeMillis();
        RecordHistogram.recordMediumTimesHistogram(
                "Omnibox.FocusToOpenTimeAnyPopupState3", activationTime - mUrlFocusTime);

        GURL url = updateSuggestionUrlIfNeeded(suggestion, matchPosition, !inVisibleSuggestionList);

        // loadUrl modifies AutocompleteController's state clearing the native
        // AutocompleteResults needed by onSuggestionsSelected. Therefore,
        // loadUrl should should be invoked last.
        int transition = suggestion.getTransition();
        int type = suggestion.getType();
        String currentPageUrl = mDataProvider.getCurrentUrl();
        WebContents webContents =
                mDataProvider.hasTab() ? mDataProvider.getTab().getWebContents() : null;
        long elapsedTimeSinceModified = mNewOmniboxEditSessionTimestamp > 0
                ? (SystemClock.elapsedRealtime() - mNewOmniboxEditSessionTimestamp)
                : -1;
        boolean shouldSkipNativeLog = mShowCachedZeroSuggestResults
                && (mDeferredOnSelection != null) && !mDeferredOnSelection.shouldLog();
        if (!shouldSkipNativeLog) {
            int autocompleteLength = mUrlBarEditingTextProvider.getTextWithAutocomplete().length()
                    - mUrlBarEditingTextProvider.getTextWithoutAutocomplete().length();
            int pageClassification =
                    mDataProvider.getPageClassification(mDelegate.didFocusUrlFromFakebox());
            mAutocomplete.onSuggestionSelected(matchPosition, suggestion.hashCode(), type,
                    currentPageUrl, pageClassification, elapsedTimeSinceModified,
                    autocompleteLength, webContents);
        }
        if (((transition & PageTransition.CORE_MASK) == PageTransition.TYPED)
                && TextUtils.equals(url.getSpec(), mDataProvider.getCurrentUrl())) {
            // When the user hit enter on the existing permanent URL, treat it like a
            // reload for scoring purposes.  We could detect this by just checking
            // user_input_in_progress_, but it seems better to treat "edits" that end
            // up leaving the URL unchanged (e.g. deleting the last character and then
            // retyping it) as reloads too.  We exclude non-TYPED transitions because if
            // the transition is GENERATED, the user input something that looked
            // different from the current URL, even if it wound up at the same place
            // (e.g. manually retyping the same search query), and it seems wrong to
            // treat this as a reload.
            transition = PageTransition.RELOAD;
        } else if (((transition & PageTransition.CORE_MASK) == PageTransition.GENERATED)
                && TextUtils.equals(
                        suggestion.getFillIntoEdit(), mDataProvider.getDisplaySearchTerms())) {
            // When the omnibox is displaying the default search provider search terms,
            // the user focuses the omnibox, and hits Enter without refining the search
            // terms, we should classify this transition as a RELOAD.
            transition = PageTransition.RELOAD;
        } else if (type == OmniboxSuggestionType.URL_WHAT_YOU_TYPED
                && mUrlBarEditingTextProvider.wasLastEditPaste()) {
            // It's important to use the page transition from the suggestion or we might end
            // up saving generated URLs as typed URLs, which would then pollute the subsequent
            // omnibox results. There is one special case where the suggestion text was pasted,
            // where we want the transition type to be LINK.

            transition = PageTransition.LINK;
        }

        if (suggestion.getType() == OmniboxSuggestionType.CLIPBOARD_IMAGE) {
            mDelegate.loadUrlWithPostData(url.getSpec(), transition, inputStart,
                    suggestion.getPostContentType(), suggestion.getPostData());
            return;
        }
        mDelegate.loadUrl(url.getSpec(), transition, inputStart);
    }

    /**
     * Make a zero suggest request if:
     * - Native is loaded.
     * - The URL bar has focus.
     * - The the tab/overview is not incognito.
     */
    private void startZeroSuggest() {
        // Reset "edited" state in the omnibox if zero suggest is triggered -- new edits
        // now count as a new session.
        mHasStartedNewOmniboxEditSession = false;
        mNewOmniboxEditSessionTimestamp = -1;

        if (mNativeInitialized && mDelegate.isUrlBarFocused()
                && (mDataProvider.hasTab() || mDataProvider.isInOverviewAndShowingOmnibox())) {
            int pageClassification =
                    mDataProvider.getPageClassification(mDelegate.didFocusUrlFromFakebox());
            mAutocomplete.startZeroSuggest(mDataProvider.getProfile(),
                    mUrlBarEditingTextProvider.getTextWithAutocomplete(),
                    mDataProvider.getCurrentUrl(), pageClassification, mDataProvider.getTitle());
        }
    }

    /**
     * Update whether the omnibox suggestions are visible.
     */
    private void updateOmniboxSuggestionsVisibility() {
        boolean shouldBeVisible =
                getSuggestionVisibilityState() == SuggestionVisibilityState.ALLOWED
                && getSuggestionCount() > 0;
        boolean wasVisible = mListPropertyModel.get(SuggestionListProperties.VISIBLE);
        mListPropertyModel.set(SuggestionListProperties.VISIBLE, shouldBeVisible);
        if (shouldBeVisible && !wasVisible) {
            mIgnoreOmniboxItemSelection = true; // Reset to default value.
        }
    }

    /**
     * Hides the omnibox suggestion popup.
     *
     * <p>
     * Signals the autocomplete controller to stop generating omnibox suggestions.
     *
     * @see AutocompleteController#stop(boolean)
     */
    private void hideSuggestions() {
        if (mAutocomplete == null || !mNativeInitialized) return;

        stopAutocomplete(true);

        getSuggestionModelList().clear();
        mViewInfoList.clear();
        mAutocompleteResult = new AutocompleteResult(null, null);
        updateOmniboxSuggestionsVisibility();
    }

    /**
     * Signals the autocomplete controller to stop generating omnibox suggestions and cancels the
     * queued task to start the autocomplete controller, if any.
     *
     * @param clear Whether to clear the most recent autocomplete results.
     */
    private void stopAutocomplete(boolean clear) {
        if (mAutocomplete != null) mAutocomplete.stop(clear);
        cancelPendingAutocompleteStart();
    }

    /**
     * Cancels the queued task to start the autocomplete controller, if any.
     */
    @VisibleForTesting
    void cancelPendingAutocompleteStart() {
        if (mRequestSuggestions != null) {
            // There is a request for suggestions either waiting for the native side
            // to start, or on the message queue. Remove it from wherever it is.
            if (!mDeferredNativeRunnables.remove(mRequestSuggestions)) {
                mHandler.removeCallbacks(mRequestSuggestions);
            }
            mRequestSuggestions = null;
        }
    }

    /**
     * Trigger autocomplete for the given query.
     */
    void startAutocompleteForQuery(String query) {
        stopAutocomplete(false);
        if (mDataProvider.hasTab()) {
            mAutocomplete.start(mDataProvider.getProfile(), mDataProvider.getCurrentUrl(),
                    mDataProvider.getPageClassification(false), query, -1, false, null);
        }
    }

    /**
     * Sets the autocomplete controller for the location bar.
     *
     * @param controller The controller that will handle autocomplete/omnibox suggestions.
     * @note Only used for testing.
     */
    @VisibleForTesting
    public void setAutocompleteController(AutocompleteController controller) {
        if (mAutocomplete != null) stopAutocomplete(true);
        mAutocomplete = controller;
    }

    private abstract static class DeferredOnSelectionRunnable implements Runnable {
        protected final OmniboxSuggestion mSuggestion;
        protected final int mPosition;
        protected boolean mShouldLog;

        public DeferredOnSelectionRunnable(OmniboxSuggestion suggestion, int position) {
            this.mSuggestion = suggestion;
            this.mPosition = position;
        }

        /**
         * Set whether the selection matches with native results for logging to make sense.
         * @param log Whether the selection should be logged in native code.
         */
        public void setShouldLog(boolean log) {
            mShouldLog = log;
        }

        /**
         * @return Whether the selection should be logged in native code.
         */
        public boolean shouldLog() {
            return mShouldLog;
        }
    }

    /**
     * Respond to Suggestion list height change and update list of presented suggestions.
     *
     * This typically happens as a result of soft keyboard being shown or hidden.
     *
     * @param newHeightPx New height of the suggestion list in pixels.
     */
    @Override
    public void onSuggestionDropdownHeightChanged(@Px int newHeightPx) {
        if (!mEnableAdaptiveSuggestionsCount) return;
        mMaximumSuggestionsListHeight = newHeightPx;
        updateSuggestionsList(mMaximumSuggestionsListHeight);
    }

    @Override
    public void onSuggestionDropdownScroll() {
        if (!shouldShowSoftKeyboard()) {
            mDelegate.setKeyboardVisibility(false);
        }
    }
}
