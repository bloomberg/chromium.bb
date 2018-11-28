// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.os.Handler;
import android.os.SystemClock;
import android.support.annotation.IntDef;
import android.support.annotation.Nullable;
import android.text.Spannable;
import android.text.SpannableString;
import android.text.TextUtils;
import android.text.style.StyleSpan;
import android.util.Pair;
import android.util.TypedValue;
import android.view.View;
import android.widget.TextView;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.modaldialog.DialogDismissalCause;
import org.chromium.chrome.browser.modaldialog.ModalDialogManager;
import org.chromium.chrome.browser.modaldialog.ModalDialogProperties;
import org.chromium.chrome.browser.modaldialog.ModalDialogView;
import org.chromium.chrome.browser.modaldialog.ModalDialogViewBinder;
import org.chromium.chrome.browser.modelutil.PropertyModel;
import org.chromium.chrome.browser.modelutil.PropertyModelChangeProcessor;
import org.chromium.chrome.browser.omnibox.LocationBarVoiceRecognitionHandler;
import org.chromium.chrome.browser.omnibox.MatchClassificationStyle;
import org.chromium.chrome.browser.omnibox.OmniboxSuggestionType;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController.OnSuggestionsReceivedListener;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteCoordinator.AutocompleteDelegate;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestion.MatchClassification;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionView.SuggestionViewDelegate;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionViewProperties.SuggestionIcon;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionViewProperties.SuggestionTextContainer;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.toolbar.ToolbarDataProvider;
import org.chromium.components.omnibox.SuggestionAnswer;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.WindowAndroid;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Handles updating the model state for the currently visible omnibox suggestions.
 */
class AutocompleteMediator implements OnSuggestionsReceivedListener {
    private static final String TAG = "cr_Autocomplete";

    // Delay triggering the omnibox results upon key press to allow the location bar to repaint
    // with the new characters.
    private static final long OMNIBOX_SUGGESTION_START_DELAY_MS = 30;

    private final Context mContext;
    private final AutocompleteDelegate mDelegate;
    private final UrlBarEditingTextStateProvider mUrlBarEditingTextProvider;
    private final PropertyModel mListPropertyModel;
    private final List<Pair<OmniboxSuggestion, PropertyModel>> mCurrentModels;
    private final Map<String, List<PropertyModel>> mPendingAnswerRequestUrls;
    private final AnswersImageFetcher mImageFetcher;
    private final List<Runnable> mDeferredNativeRunnables = new ArrayList<Runnable>();
    private final Handler mHandler;

    private ToolbarDataProvider mDataProvider;
    private boolean mNativeInitialized;
    private AutocompleteController mAutocomplete;

    @IntDef({SuggestionVisibilityState.DISALLOWED, SuggestionVisibilityState.PENDING_ALLOW,
            SuggestionVisibilityState.ALLOWED})
    @Retention(RetentionPolicy.SOURCE)
    private @interface SuggestionVisibilityState {
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

    private boolean mUseDarkColors = true;
    private int mLayoutDirection;
    private boolean mPreventSuggestionListPropertyChanges;
    private long mLastActionUpTimestamp;
    private boolean mIgnoreOmniboxItemSelection = true;
    private float mMaxRequiredWidth;
    private float mMaxMatchContentsWidth;

    private WindowAndroid mWindowAndroid;
    private ModalDialogView mSuggestionDeleteDialog;

    public AutocompleteMediator(Context context, AutocompleteDelegate delegate,
            UrlBarEditingTextStateProvider textProvider, PropertyModel listPropertyModel) {
        mContext = context;
        mDelegate = delegate;
        mUrlBarEditingTextProvider = textProvider;
        mListPropertyModel = listPropertyModel;
        mCurrentModels = new ArrayList<>();
        mPendingAnswerRequestUrls = new HashMap<>();
        mImageFetcher = new AnswersImageFetcher();
        mAutocomplete = new AutocompleteController(this);
        mHandler = new Handler();
    }

    /**
     * Clear and notify observers that all suggestions are gone.
     */
    public void clearSuggestions() {
        mCurrentModels.clear();
        notifyPropertyModelsChanged();
    }

    /**
     * @return The number of current autocomplete suggestions.
     */
    public int getSuggestionCount() {
        return mCurrentModels.size();
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
        return mCurrentModels.get(index).first;
    }

    private void notifyPropertyModelsChanged() {
        if (mPreventSuggestionListPropertyChanges) return;
        List<Pair<Integer, PropertyModel>> models = new ArrayList<>(mCurrentModels.size());
        for (int i = 0; i < mCurrentModels.size(); i++) {
            models.add(new Pair<>(OmniboxSuggestionUiType.DEFAULT, mCurrentModels.get(i).second));
        }
        mListPropertyModel.set(SuggestionListProperties.SUGGESTION_MODELS, models);
    }

    private void populateModelForSuggestion(
            PropertyModel model, OmniboxSuggestion suggestion, int position) {
        maybeFetchAnswerIcon(suggestion, model);

        model.set(SuggestionViewProperties.SUGGESTION_ICON_TYPE, SuggestionIcon.UNDEFINED);
        model.set(SuggestionViewProperties.DELEGATE,
                createSuggestionViewDelegate(suggestion, position));
        model.set(SuggestionViewProperties.USE_DARK_COLORS, mUseDarkColors);
        model.set(SuggestionViewProperties.LAYOUT_DIRECTION, mLayoutDirection);

        // Suggestions with attached answers are rendered with rich results regardless of which
        // suggestion type they are.
        if (suggestion.hasAnswer()) {
            setStateForAnswerSuggestion(model, suggestion.getAnswer());
        } else {
            setStateForTextSuggestion(model, suggestion);
        }
    }

    private static boolean applyHighlightToMatchRegions(
            Spannable str, List<MatchClassification> classifications) {
        boolean hasMatch = false;
        for (int i = 0; i < classifications.size(); i++) {
            MatchClassification classification = classifications.get(i);
            if ((classification.style & MatchClassificationStyle.MATCH)
                    == MatchClassificationStyle.MATCH) {
                int matchStartIndex = classification.offset;
                int matchEndIndex;
                if (i == classifications.size() - 1) {
                    matchEndIndex = str.length();
                } else {
                    matchEndIndex = classifications.get(i + 1).offset;
                }
                matchStartIndex = Math.min(matchStartIndex, str.length());
                matchEndIndex = Math.min(matchEndIndex, str.length());

                hasMatch = true;
                // Bold the part of the URL that matches the user query.
                str.setSpan(new StyleSpan(android.graphics.Typeface.BOLD), matchStartIndex,
                        matchEndIndex, Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
            }
        }
        return hasMatch;
    }

    /**
     * Get the first line for a text based omnibox suggestion.
     * @param suggestion The item containing the suggestion data.
     * @param showDescriptionIfPresent Whether to show the description text of the suggestion if
     *                                 the item contains valid data.
     * @param shouldHighlight Whether the query should be highlighted.
     * @return The first line of text.
     */
    private Spannable getSuggestedQuery(OmniboxSuggestion suggestion,
            boolean showDescriptionIfPresent, boolean shouldHighlight) {
        String userQuery = mUrlBarEditingTextProvider.getTextWithoutAutocomplete();
        String suggestedQuery = null;
        List<MatchClassification> classifications;
        if (showDescriptionIfPresent && !TextUtils.isEmpty(suggestion.getUrl())
                && !TextUtils.isEmpty(suggestion.getDescription())) {
            suggestedQuery = suggestion.getDescription();
            classifications = suggestion.getDescriptionClassifications();
        } else {
            suggestedQuery = suggestion.getDisplayText();
            classifications = suggestion.getDisplayTextClassifications();
        }
        if (suggestedQuery == null) {
            assert false : "Invalid suggestion sent with no displayable text";
            suggestedQuery = "";
            classifications = new ArrayList<MatchClassification>();
            classifications.add(new MatchClassification(0, MatchClassificationStyle.NONE));
        }

        if (suggestion.getType() == OmniboxSuggestionType.SEARCH_SUGGEST_TAIL) {
            String fillIntoEdit = suggestion.getFillIntoEdit();
            // Data sanity checks.
            if (fillIntoEdit.startsWith(userQuery) && fillIntoEdit.endsWith(suggestedQuery)
                    && fillIntoEdit.length() < userQuery.length() + suggestedQuery.length()) {
                final String ellipsisPrefix = "\u2026 ";
                suggestedQuery = ellipsisPrefix + suggestedQuery;

                // Offset the match classifications by the length of the ellipsis prefix to ensure
                // the highlighting remains correct.
                for (int i = 0; i < classifications.size(); i++) {
                    classifications.set(i,
                            new MatchClassification(
                                    classifications.get(i).offset + ellipsisPrefix.length(),
                                    classifications.get(i).style));
                }
                classifications.add(0, new MatchClassification(0, MatchClassificationStyle.NONE));
            }
        }

        Spannable str = SpannableString.valueOf(suggestedQuery);
        if (shouldHighlight) applyHighlightToMatchRegions(str, classifications);
        return str;
    }

    private void setStateForTextSuggestion(PropertyModel model, OmniboxSuggestion suggestion) {
        int suggestionType = suggestion.getType();
        @SuggestionIcon
        int suggestionIcon;
        Spannable textLine1;

        Spannable textLine2;
        int textLine2Color = 0;
        int textLine2Direction = View.TEXT_DIRECTION_INHERIT;
        if (suggestion.isUrlSuggestion()) {
            suggestionIcon = SuggestionIcon.GLOBE;
            if (suggestion.isStarred()) {
                suggestionIcon = SuggestionIcon.BOOKMARK;
            } else if (suggestionType == OmniboxSuggestionType.HISTORY_URL) {
                suggestionIcon = SuggestionIcon.HISTORY;
            }
            boolean urlHighlighted = false;
            if (!TextUtils.isEmpty(suggestion.getUrl())) {
                Spannable str = SpannableString.valueOf(suggestion.getDisplayText());
                urlHighlighted = applyHighlightToMatchRegions(
                        str, suggestion.getDisplayTextClassifications());
                textLine2 = str;
                textLine2Color = SuggestionViewViewBinder.getStandardUrlColor(
                        mContext, model.get(SuggestionViewProperties.USE_DARK_COLORS));
                textLine2Direction = View.TEXT_DIRECTION_LTR;
            } else {
                textLine2 = null;
            }
            textLine1 = getSuggestedQuery(suggestion, true, !urlHighlighted);
        } else {
            suggestionIcon = SuggestionIcon.MAGNIFIER;
            if (suggestionType == OmniboxSuggestionType.VOICE_SUGGEST) {
                suggestionIcon = SuggestionIcon.VOICE;
            } else if ((suggestionType == OmniboxSuggestionType.SEARCH_SUGGEST_PERSONALIZED)
                    || (suggestionType == OmniboxSuggestionType.SEARCH_HISTORY)) {
                // Show history icon for suggestions based on user queries.
                suggestionIcon = SuggestionIcon.HISTORY;
            }
            textLine1 = getSuggestedQuery(suggestion, false, true);
            if ((suggestionType == OmniboxSuggestionType.SEARCH_SUGGEST_ENTITY)
                    || (suggestionType == OmniboxSuggestionType.SEARCH_SUGGEST_PROFILE)) {
                textLine2 = SpannableString.valueOf(suggestion.getDescription());
                textLine2Color = SuggestionViewViewBinder.getStandardFontColor(
                        mContext, model.get(SuggestionViewProperties.USE_DARK_COLORS));
                textLine2Direction = View.TEXT_DIRECTION_INHERIT;
            } else {
                textLine2 = null;
            }
        }

        model.set(SuggestionViewProperties.IS_ANSWER, false);
        model.set(SuggestionViewProperties.HAS_ANSWER_IMAGE, false);
        model.set(SuggestionViewProperties.ANSWER_IMAGE, null);

        model.set(
                SuggestionViewProperties.TEXT_LINE_1_TEXT, new SuggestionTextContainer(textLine1));
        model.set(SuggestionViewProperties.TEXT_LINE_1_SIZING,
                Pair.create(TypedValue.COMPLEX_UNIT_PX,
                        mContext.getResources().getDimension(
                                R.dimen.omnibox_suggestion_first_line_text_size)));

        model.set(
                SuggestionViewProperties.TEXT_LINE_2_TEXT, new SuggestionTextContainer(textLine2));
        model.set(SuggestionViewProperties.TEXT_LINE_2_TEXT_COLOR, textLine2Color);
        model.set(SuggestionViewProperties.TEXT_LINE_2_TEXT_DIRECTION, textLine2Direction);
        model.set(SuggestionViewProperties.TEXT_LINE_2_SIZING,
                Pair.create(TypedValue.COMPLEX_UNIT_PX,
                        mContext.getResources().getDimension(
                                R.dimen.omnibox_suggestion_second_line_text_size)));
        model.set(SuggestionViewProperties.TEXT_LINE_2_MAX_LINES, 1);

        boolean sameAsTyped =
                mUrlBarEditingTextProvider.getTextWithoutAutocomplete().trim().equalsIgnoreCase(
                        suggestion.getDisplayText());
        model.set(SuggestionViewProperties.REFINABLE, !sameAsTyped);

        model.set(SuggestionViewProperties.SUGGESTION_ICON_TYPE, suggestionIcon);
    }

    private static int parseNumAnswerLines(List<SuggestionAnswer.TextField> textFields) {
        for (int i = 0; i < textFields.size(); i++) {
            if (textFields.get(i).hasNumLines()) {
                return Math.min(3, textFields.get(i).getNumLines());
            }
        }
        return -1;
    }

    /**
     * Sets both lines of the Omnibox suggestion based on an Answers in Suggest result.
     */
    private void setStateForAnswerSuggestion(PropertyModel model, SuggestionAnswer answer) {
        float density = mContext.getResources().getDisplayMetrics().density;
        SuggestionAnswer.ImageLine firstLine = answer.getFirstLine();
        SuggestionAnswer.ImageLine secondLine = answer.getSecondLine();
        int numAnswerLines = parseNumAnswerLines(secondLine.getTextFields());
        if (numAnswerLines == -1) numAnswerLines = 1;
        model.set(SuggestionViewProperties.IS_ANSWER, true);

        model.set(SuggestionViewProperties.TEXT_LINE_1_SIZING,
                Pair.create(TypedValue.COMPLEX_UNIT_SP,
                        (float) AnswerTextBuilder.getMaxTextHeightSp(firstLine)));
        model.set(SuggestionViewProperties.TEXT_LINE_1_TEXT,
                new SuggestionTextContainer(AnswerTextBuilder.buildSpannable(firstLine, density)));

        model.set(SuggestionViewProperties.TEXT_LINE_2_SIZING,
                Pair.create(TypedValue.COMPLEX_UNIT_SP,
                        (float) AnswerTextBuilder.getMaxTextHeightSp(secondLine)));
        model.set(SuggestionViewProperties.TEXT_LINE_2_TEXT,
                new SuggestionTextContainer(AnswerTextBuilder.buildSpannable(secondLine, density)));
        model.set(SuggestionViewProperties.TEXT_LINE_2_MAX_LINES, numAnswerLines);
        model.set(SuggestionViewProperties.TEXT_LINE_2_TEXT_COLOR,
                SuggestionViewViewBinder.getStandardFontColor(
                        mContext, model.get(SuggestionViewProperties.USE_DARK_COLORS)));
        model.set(SuggestionViewProperties.TEXT_LINE_2_TEXT_DIRECTION, View.TEXT_DIRECTION_INHERIT);

        model.set(SuggestionViewProperties.HAS_ANSWER_IMAGE, secondLine.hasImage());

        model.set(SuggestionViewProperties.REFINABLE, true);
        model.set(SuggestionViewProperties.SUGGESTION_ICON_TYPE, SuggestionIcon.MAGNIFIER);
    }

    private void maybeFetchAnswerIcon(OmniboxSuggestion suggestion, PropertyModel model) {
        ThreadUtils.assertOnUiThread();

        // Attempting to fetch answer data before we have a profile to request it for.
        if (mDataProvider == null) return;

        if (!suggestion.hasAnswer()) return;
        final String url = suggestion.getAnswer().getSecondLine().getImage();
        if (url == null) return;

        // Do not make duplicate answer image requests for the same URL (to avoid generating
        // duplicate bitmaps for the same image).
        if (mPendingAnswerRequestUrls.containsKey(url)) {
            mPendingAnswerRequestUrls.get(url).add(model);
            return;
        }

        List<PropertyModel> models = new ArrayList<>();
        models.add(model);
        mPendingAnswerRequestUrls.put(url, models);
        mImageFetcher.requestAnswersImage(
                mDataProvider.getProfile(), url, new AnswersImageFetcher.AnswersImageObserver() {
                    @Override
                    public void onAnswersImageChanged(Bitmap bitmap) {
                        ThreadUtils.assertOnUiThread();

                        List<PropertyModel> models = mPendingAnswerRequestUrls.remove(url);
                        boolean didUpdate = false;
                        for (int i = 0; i < models.size(); i++) {
                            PropertyModel model = models.get(i);
                            if (!isActiveModel(model)) continue;
                            model.set(SuggestionViewProperties.ANSWER_IMAGE, bitmap);
                            didUpdate = true;
                        }
                        if (didUpdate) notifyPropertyModelsChanged();
                    }
                });
    }

    private boolean isActiveModel(PropertyModel model) {
        for (int i = 0; i < mCurrentModels.size(); i++) {
            if (mCurrentModels.get(i).second.equals(model)) return true;
        }
        return false;
    }

    /**
     * Sets the data provider for the toolbar.
     */
    void setToolbarDataProvider(ToolbarDataProvider provider) {
        mDataProvider = provider;
    }

    /** Set the WindowAndroid instance associated with the containing Activity. */
    void setWindowAndroid(WindowAndroid window) {
        mWindowAndroid = window;
    }

    /**
     * Sets the layout direction to be used for any new suggestion views.
     * @see View#setLayoutDirection(int)
     */
    void setLayoutDirection(int layoutDirection) {
        mLayoutDirection = layoutDirection;
        for (int i = 0; i < mCurrentModels.size(); i++) {
            PropertyModel model = mCurrentModels.get(i).second;
            model.set(SuggestionViewProperties.LAYOUT_DIRECTION, mLayoutDirection);
        }
        if (!mCurrentModels.isEmpty()) notifyPropertyModelsChanged();
    }

    /**
     * Specifies the visual state to be used by the suggestions.
     * @param useDarkColors Whether dark colors should be used for fonts and icons.
     */
    void setUseDarkColors(boolean useDarkColors) {
        mUseDarkColors = useDarkColors;
        mListPropertyModel.set(SuggestionListProperties.USE_DARK_BACKGROUND, !useDarkColors);
        for (int i = 0; i < mCurrentModels.size(); i++) {
            PropertyModel model = mCurrentModels.get(i).second;
            model.set(SuggestionViewProperties.USE_DARK_COLORS, mUseDarkColors);
        }
        if (!mCurrentModels.isEmpty()) notifyPropertyModelsChanged();
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

        for (Runnable deferredRunnable : mDeferredNativeRunnables) {
            mHandler.post(deferredRunnable);
        }
        mDeferredNativeRunnables.clear();
    }

    /** @see org.chromium.chrome.browser.omnibox.UrlFocusChangeListener#onUrlFocusChange(boolean) */
    void onUrlFocusChange(boolean hasFocus) {
        if (hasFocus) {
            mSuggestionVisibilityState = SuggestionVisibilityState.PENDING_ALLOW;
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
            mSuggestionVisibilityState = SuggestionVisibilityState.DISALLOWED;
            mHasStartedNewOmniboxEditSession = false;
            mNewOmniboxEditSessionTimestamp = -1;
            // Prevent any upcoming omnibox suggestions from showing once a URL is loaded (and as
            // a consequence the omnibox is unfocused).
            hideSuggestions();
            mImageFetcher.clearCache();
        }
    }

    /**
     * @see
     * org.chromium.chrome.browser.omnibox.UrlFocusChangeListener#onUrlAnimationFinished(boolean)
     */
    void onUrlAnimationFinished(boolean hasFocus) {
        mSuggestionVisibilityState =
                hasFocus ? SuggestionVisibilityState.ALLOWED : SuggestionVisibilityState.DISALLOWED;
        updateOmniboxSuggestionsVisibility();
    }

    /**
     * Updates the profile used for generating autocomplete suggestions.
     * @param profile The profile to be used.
     */
    void setAutocompleteProfile(Profile profile) {
        mAutocomplete.setProfile(profile);
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
    void onVoiceResults(@Nullable List<LocationBarVoiceRecognitionHandler.VoiceResult> results) {
        mAutocomplete.onVoiceResults(results);
    }

    /**
     * @return The current native pointer to the autocomplete results.
     */
    // TODO(tedchoc): Figure out how to remove this.
    long getCurrentNativeAutocompleteResult() {
        return mAutocomplete.getCurrentNativeAutocompleteResult();
    }

    private SuggestionViewDelegate createSuggestionViewDelegate(
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
            public void onRefineSuggestion() {
                AutocompleteMediator.this.onRefineSuggestion(suggestion);
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

            @Override
            public int getAdditionalTextLine1StartPadding(TextView line1, int maxTextWidth) {
                if (!DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext)) return 0;
                if (suggestion.getType() != OmniboxSuggestionType.SEARCH_SUGGEST_TAIL) return 0;

                String fillIntoEdit = suggestion.getFillIntoEdit();
                float fullTextWidth =
                        line1.getPaint().measureText(fillIntoEdit, 0, fillIntoEdit.length());
                String query = line1.getText().toString();
                float abbreviatedTextWidth = line1.getPaint().measureText(query, 0, query.length());

                AutocompleteMediator.this.onTextWidthsUpdated(fullTextWidth, abbreviatedTextWidth);

                final float maxRequiredWidth = AutocompleteMediator.this.mMaxRequiredWidth;
                final float maxMatchContentsWidth =
                        AutocompleteMediator.this.mMaxMatchContentsWidth;
                return (int) ((maxTextWidth > maxRequiredWidth)
                                ? (fullTextWidth - abbreviatedTextWidth)
                                : Math.max(maxTextWidth - maxMatchContentsWidth, 0));
            }
        };
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
        mDelegate.hideKeyboard();
    }

    /**
     * Triggered when the user selects to refine one of the omnibox suggestions.
     * @param suggestion The suggestion selected.
     */
    private void onRefineSuggestion(OmniboxSuggestion suggestion) {
        stopAutocomplete(false);
        boolean isUrlSuggestion = suggestion.isUrlSuggestion();
        String refineText = suggestion.getFillIntoEdit();
        if (!isUrlSuggestion) refineText = TextUtils.concat(refineText, " ").toString();

        mDelegate.setOmniboxEditingText(refineText);
        onTextChangedForAutocomplete();
        if (isUrlSuggestion) {
            RecordUserAction.record("MobileOmniboxRefineSuggestion.Url");
        } else {
            RecordUserAction.record("MobileOmniboxRefineSuggestion.Search");
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

        ModalDialogView.Controller dialogController = new ModalDialogView.Controller() {
            @Override
            public void onDismiss(int dismissalCause) {
                mSuggestionDeleteDialog = null;
            }

            @Override
            public void onClick(int buttonType) {
                if (buttonType == ModalDialogView.ButtonType.POSITIVE) {
                    RecordUserAction.record("MobileOmniboxDeleteRequested");
                    mAutocomplete.deleteSuggestion(position, suggestion.hashCode());
                    manager.dismissDialog(
                            mSuggestionDeleteDialog, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                } else if (buttonType == ModalDialogView.ButtonType.NEGATIVE) {
                    manager.dismissDialog(
                            mSuggestionDeleteDialog, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
                }
            }
        };

        Resources resources = mContext.getResources();
        PropertyModel model =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, dialogController)
                        .with(ModalDialogProperties.TITLE, suggestion.getDisplayText())
                        .with(ModalDialogProperties.MESSAGE, resources,
                                R.string.omnibox_confirm_delete)
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, resources, R.string.ok)
                        .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, resources,
                                R.string.cancel)
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                        .build();

        mSuggestionDeleteDialog = new ModalDialogView(mContext);
        PropertyModelChangeProcessor.create(
                model, mSuggestionDeleteDialog, new ModalDialogViewBinder());

        // Prevent updates to the shown omnibox suggestions list while the dialog is open.
        stopAutocomplete(false);
        manager.showDialog(mSuggestionDeleteDialog, ModalDialogManager.ModalDialogType.APP);
    }

    /**
     * Triggered when the user navigates to one of the suggestions without clicking on it.
     * @param suggestion The suggestion that was selected.
     */
    void onSetUrlToSuggestion(OmniboxSuggestion suggestion) {
        mDelegate.setOmniboxEditingText(suggestion.getFillIntoEdit());
    }

    /**
     * Triggered when text width information is updated.
     * These values should be used to calculate max text widths.
     * @param requiredWidth a new required width.
     * @param matchContentsWidth a new match contents width.
     */
    private void onTextWidthsUpdated(float requiredWidth, float matchContentsWidth) {
        mMaxRequiredWidth = Math.max(mMaxRequiredWidth, requiredWidth);
        mMaxMatchContentsWidth = Math.max(mMaxMatchContentsWidth, matchContentsWidth);
    }

    /**
     * Updates the maximum widths required to render the suggestions.
     * This is needed for infinite suggestions where we try to vertically align the leading
     * ellipsis.
     */
    private void resetMaxTextWidths() {
        mMaxRequiredWidth = 0;
        mMaxMatchContentsWidth = 0;
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
    @SuppressWarnings("ReferenceEquality")
    private String updateSuggestionUrlIfNeeded(
            OmniboxSuggestion suggestion, int selectedIndex, boolean skipCheck) {
        // Only called once we have suggestions, and don't have a listener though which we can
        // receive suggestions until the native side is ready, so this is safe
        assert mNativeInitialized
            : "updateSuggestionUrlIfNeeded called before native initialization";

        if (suggestion.getType() == OmniboxSuggestionType.VOICE_SUGGEST) return suggestion.getUrl();

        int verifiedIndex = -1;
        if (!skipCheck) {
            if (getSuggestionCount() > selectedIndex
                    && getSuggestionAt(selectedIndex) == suggestion) {
                verifiedIndex = selectedIndex;
            } else {
                // Underlying omnibox results may have changed since the selection was made,
                // find the suggestion item, if possible.
                for (int i = 0; i < getSuggestionCount(); i++) {
                    if (suggestion.equals(getSuggestionAt(i))) {
                        verifiedIndex = i;
                        break;
                    }
                }
            }
        }

        // If we do not have the suggestion as part of our results, skip the URL update.
        if (verifiedIndex == -1) return suggestion.getUrl();

        // TODO(mariakhomenko): Ideally we want to update match destination URL with new aqs
        // for query in the omnibox and voice suggestions, but it's currently difficult to do.
        long elapsedTimeSinceInputChange = mNewOmniboxEditSessionTimestamp > 0
                ? (SystemClock.elapsedRealtime() - mNewOmniboxEditSessionTimestamp)
                : -1;
        String updatedUrl = mAutocomplete.updateMatchDestinationUrlWithQueryFormulationTime(
                verifiedIndex, suggestion.hashCode(), elapsedTimeSinceInputChange);

        return updatedUrl == null ? suggestion.getUrl() : updatedUrl;
    }

    /**
     * Notifies the autocomplete system that the text has changed that drives autocomplete and the
     * autocomplete suggestions should be updated.
     */
    public void onTextChangedForAutocomplete() {
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
        if (TextUtils.isEmpty(mUrlBarEditingTextProvider.getTextWithoutAutocomplete())) {
            // crbug.com/764749
            Log.w(TAG, "onTextChangedForAutocomplete: url is empty");
            hideSuggestions();
            startZeroSuggest();
        } else {
            assert mRequestSuggestions == null : "Multiple omnibox requests in flight.";
            mRequestSuggestions = () -> {
                String textWithoutAutocomplete =
                        mUrlBarEditingTextProvider.getTextWithoutAutocomplete();
                boolean preventAutocomplete = !mUrlBarEditingTextProvider.shouldAutocomplete();
                mRequestSuggestions = null;

                if (!mDataProvider.hasTab()) {
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
                mAutocomplete.start(profile, mDataProvider.getCurrentUrl(), textWithoutAutocomplete,
                        cursorPosition, preventAutocomplete, mDelegate.didFocusUrlFromFakebox());
            };
            if (mNativeInitialized) {
                mHandler.postDelayed(mRequestSuggestions, OMNIBOX_SUGGESTION_START_DELAY_MS);
            } else {
                mDeferredNativeRunnables.add(mRequestSuggestions);
            }
        }

        mDelegate.onUrlTextChanged();
    }

    @Override
    public void onSuggestionsReceived(
            List<OmniboxSuggestion> newSuggestions, String inlineAutocompleteText) {
        if (mShouldPreventOmniboxAutocomplete
                || mSuggestionVisibilityState == SuggestionVisibilityState.DISALLOWED) {
            return;
        }

        // This is a callback from a listener that is set up by onNativeLibraryReady,
        // so can only be called once the native side is set up unless we are showing
        // cached java-only suggestions.
        assert mNativeInitialized
                || mShowCachedZeroSuggestResults
            : "Native suggestions received before native side intialialized";

        if (mDeferredOnSelection != null) {
            mDeferredOnSelection.setShouldLog(newSuggestions.size() > mDeferredOnSelection.mPosition
                    && mDeferredOnSelection.mSuggestion.equals(
                               newSuggestions.get(mDeferredOnSelection.mPosition)));
            mDeferredOnSelection.run();
            mDeferredOnSelection = null;
        }
        String userText = mUrlBarEditingTextProvider.getTextWithoutAutocomplete();
        mUrlTextAfterSuggestionsReceived = userText + inlineAutocompleteText;

        if (mCurrentModels.size() == newSuggestions.size()) {
            boolean sameSuggestions = true;
            for (int i = 0; i < mCurrentModels.size(); i++) {
                if (!mCurrentModels.get(i).first.equals(newSuggestions.get(i))) {
                    sameSuggestions = false;
                    break;
                }
            }
            if (sameSuggestions) return;
        }

        // Show the suggestion list.
        resetMaxTextWidths();
        // Ensure the list is fully replaced before broadcasting any change notifications.
        mPreventSuggestionListPropertyChanges = true;
        mCurrentModels.clear();
        for (int i = 0; i < newSuggestions.size(); i++) {
            PropertyModel model = new PropertyModel(SuggestionViewProperties.ALL_KEYS);
            OmniboxSuggestion suggestion = newSuggestions.get(i);
            mCurrentModels.add(Pair.create(suggestion, model));
            // Before populating the model, add it to the list of current models.  If the suggestion
            // has an image and the image was already cached, it will be updated synchronously and
            // the model will only have the image populated if it is tracked as a current model.
            populateModelForSuggestion(model, suggestion, i);
        }
        mPreventSuggestionListPropertyChanges = false;
        notifyPropertyModelsChanged();

        if (mListPropertyModel.get(SuggestionListProperties.VISIBLE) && getSuggestionCount() == 0) {
            hideSuggestions();
        }
        mDelegate.onSuggestionsChanged(inlineAutocompleteText);

        updateOmniboxSuggestionsVisibility();
    }

    /**
     * Load the url corresponding to the typed omnibox text.
     * @param eventTime The timestamp the load was triggered by the user.
     */
    void loadTypedOmniboxText(long eventTime) {
        mDelegate.hideKeyboard();

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
        String url =
                updateSuggestionUrlIfNeeded(suggestion, matchPosition, !inVisibleSuggestionList);

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
            mAutocomplete.onSuggestionSelected(matchPosition, suggestion.hashCode(), type,
                    currentPageUrl, mDelegate.didFocusUrlFromFakebox(), elapsedTimeSinceModified,
                    autocompleteLength, webContents);
        }
        if (((transition & PageTransition.CORE_MASK) == PageTransition.TYPED)
                && TextUtils.equals(url, mDataProvider.getCurrentUrl())) {
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
        } else if (type == OmniboxSuggestionType.URL_WHAT_YOU_TYPED
                && mUrlBarEditingTextProvider.wasLastEditPaste()) {
            // It's important to use the page transition from the suggestion or we might end
            // up saving generated URLs as typed URLs, which would then pollute the subsequent
            // omnibox results. There is one special case where the suggestion text was pasted,
            // where we want the transition type to be LINK.

            transition = PageTransition.LINK;
        }
        mDelegate.loadUrl(url, transition, inputStart);
    }

    /**
     * Make a zero suggest request if:
     * - Native is loaded.
     * - The URL bar has focus.
     * - The current tab is not incognito.
     */
    private void startZeroSuggest() {
        // Reset "edited" state in the omnibox if zero suggest is triggered -- new edits
        // now count as a new session.
        mHasStartedNewOmniboxEditSession = false;
        mNewOmniboxEditSessionTimestamp = -1;
        if (mNativeInitialized && mDelegate.isUrlBarFocused() && mDataProvider.hasTab()) {
            mAutocomplete.startZeroSuggest(mDataProvider.getProfile(),
                    mUrlBarEditingTextProvider.getTextWithAutocomplete(),
                    mDataProvider.getCurrentUrl(), mDataProvider.getTitle(),
                    mDelegate.didFocusUrlFromFakebox());
        }
    }

    /**
     * Update whether the omnibox suggestions are visible.
     */
    private void updateOmniboxSuggestionsVisibility() {
        boolean shouldBeVisible = mSuggestionVisibilityState == SuggestionVisibilityState.ALLOWED
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

        clearSuggestions();
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
            mAutocomplete.start(mDataProvider.getProfile(), mDataProvider.getCurrentUrl(), query,
                    -1, false, false);
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

    private static abstract class DeferredOnSelectionRunnable implements Runnable {
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
}
