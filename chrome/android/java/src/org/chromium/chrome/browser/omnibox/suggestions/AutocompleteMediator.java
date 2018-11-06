// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.content.Context;
import android.graphics.Bitmap;
import android.text.Spannable;
import android.text.SpannableString;
import android.text.TextUtils;
import android.text.style.StyleSpan;
import android.util.Pair;
import android.util.TypedValue;
import android.view.View;
import android.widget.TextView;

import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.modelutil.PropertyModel;
import org.chromium.chrome.browser.omnibox.MatchClassificationStyle;
import org.chromium.chrome.browser.omnibox.OmniboxSuggestionType;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController.OnSuggestionsReceivedListener;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestion.MatchClassification;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionView.SuggestionViewDelegate;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionViewProperties.SuggestionIcon;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionViewProperties.SuggestionTextContainer;
import org.chromium.chrome.browser.toolbar.ToolbarDataProvider;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Handles updating the model state for the currently visible omnibox suggestions.
 */
class AutocompleteMediator implements OnSuggestionsReceivedListener {
    private final Context mContext;
    private final UrlBarEditingTextStateProvider mUrlBarEditingTextProvider;
    private final PropertyModel mListPropertyModel;
    private final List<Pair<OmniboxSuggestion, PropertyModel>> mCurrentModels;
    private final Map<String, List<PropertyModel>> mPendingAnswerRequestUrls;
    private final AnswersImageFetcher mImageFetcher;

    private ToolbarDataProvider mDataProvider;
    private OmniboxSuggestionDelegate mSuggestionDelegate;
    private boolean mUseDarkColors = true;
    private int mLayoutDirection;
    private boolean mPreventSuggestionListPropertyChanges;

    public AutocompleteMediator(Context context, UrlBarEditingTextStateProvider textProvider,
            PropertyModel listPropertyModel) {
        mContext = context;
        mUrlBarEditingTextProvider = textProvider;
        mListPropertyModel = listPropertyModel;
        mCurrentModels = new ArrayList<>();
        mPendingAnswerRequestUrls = new HashMap<>();
        mImageFetcher = new AnswersImageFetcher();
    }

    @Override
    public void onSuggestionsReceived(
            List<OmniboxSuggestion> newSuggestions, String inlineAutocompleteText) {
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
        List<PropertyModel> models = new ArrayList<>(mCurrentModels.size());
        for (int i = 0; i < mCurrentModels.size(); i++) models.add(mCurrentModels.get(i).second);
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
    public void setToolbarDataProvider(ToolbarDataProvider provider) {
        mDataProvider = provider;
    }

    /**
     * Set the selection delegate for suggestion entries in the adapter.
     *
     * @param delegate The delegate for suggestion selections.
     */
    public void setSuggestionDelegate(OmniboxSuggestionDelegate delegate) {
        mSuggestionDelegate = delegate;
    }

    /**
     * Sets the layout direction to be used for any new suggestion views.
     * @see View#setLayoutDirection(int)
     */
    public void setLayoutDirection(int layoutDirection) {
        mLayoutDirection = layoutDirection;
        for (int i = 0; i < mCurrentModels.size(); i++) {
            PropertyModel model = mCurrentModels.get(i).second;
            model.set(SuggestionViewProperties.LAYOUT_DIRECTION, mLayoutDirection);
        }
        if (!mCurrentModels.isEmpty()) notifyPropertyModelsChanged();
    }

    /**
     * @return The selection delegate for suggestion entries in the adapter.
     */
    @VisibleForTesting
    public OmniboxSuggestionDelegate getSuggestionDelegate() {
        return mSuggestionDelegate;
    }

    /**
     * Specifies the visual state to be used by the suggestions.
     * @param useDarkColors Whether dark colors should be used for fonts and icons.
     */
    public void setUseDarkColors(boolean useDarkColors) {
        mUseDarkColors = useDarkColors;
        mListPropertyModel.set(SuggestionListProperties.USE_DARK_BACKGROUND, !useDarkColors);
        for (int i = 0; i < mCurrentModels.size(); i++) {
            PropertyModel model = mCurrentModels.get(i).second;
            model.set(SuggestionViewProperties.USE_DARK_COLORS, mUseDarkColors);
        }
        if (!mCurrentModels.isEmpty()) notifyPropertyModelsChanged();
    }

    private SuggestionViewDelegate createSuggestionViewDelegate(
            OmniboxSuggestion suggestion, int position) {
        return new SuggestionViewDelegate() {
            @Override
            public void onSetUrlToSuggestion() {
                mSuggestionDelegate.onSetUrlToSuggestion(suggestion);
            }

            @Override
            public void onSelection() {
                mSuggestionDelegate.onSelection(suggestion, position);
            }

            @Override
            public void onRefineSuggestion() {
                mSuggestionDelegate.onRefineSuggestion(suggestion);
            }

            @Override
            public void onLongPress() {
                mSuggestionDelegate.onLongPress(suggestion, position);
            }

            @Override
            public void onGestureUp(long timetamp) {
                mSuggestionDelegate.onGestureUp(timetamp);
            }

            @Override
            public void onGestureDown() {
                mSuggestionDelegate.onGestureDown();
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
                mSuggestionDelegate.onTextWidthsUpdated(fullTextWidth, abbreviatedTextWidth);

                final float maxRequiredWidth = mSuggestionDelegate.getMaxRequiredWidth();
                final float maxMatchContentsWidth = mSuggestionDelegate.getMaxMatchContentsWidth();
                return (int) ((maxTextWidth > maxRequiredWidth)
                                ? (fullTextWidth - abbreviatedTextWidth)
                                : Math.max(maxTextWidth - maxMatchContentsWidth, 0));
            }
        };
    }

    /**
     * Handler for actions that happen on suggestion view.
     */
    @VisibleForTesting
    public static interface OmniboxSuggestionDelegate {
        /**
         * Triggered when the user selects one of the omnibox suggestions to navigate to.
         * @param suggestion The OmniboxSuggestion which was selected.
         * @param position Position of the suggestion in the drop down view.
         */
        public void onSelection(OmniboxSuggestion suggestion, int position);

        /**
         * Triggered when the user selects to refine one of the omnibox suggestions.
         * @param suggestion The suggestion selected.
         */
        public void onRefineSuggestion(OmniboxSuggestion suggestion);

        /**
         * Triggered when the user long presses the omnibox suggestion.
         * @param suggestion The suggestion selected.
         * @param position The position of the suggestion.
         */
        public void onLongPress(OmniboxSuggestion suggestion, int position);

        /**
         * Triggered when the user navigates to one of the suggestions without clicking on it.
         * @param suggestion The suggestion that was selected.
         */
        public void onSetUrlToSuggestion(OmniboxSuggestion suggestion);

        /**
         * Triggered when the user touches the suggestion view.
         */
        public void onGestureDown();

        /**
         * Triggered when the user touch on the suggestion view finishes.
         * @param ev the event for the ACTION_UP.
         */
        public void onGestureUp(long timetamp);

        /**
         * Triggered when text width information is updated.
         * These values should be used to calculate max text widths.
         * @param requiredWidth a new required width.
         * @param matchContentsWidth a new match contents width.
         */
        public void onTextWidthsUpdated(float requiredWidth, float matchContentsWidth);

        /**
         * @return max required width for the suggestion.
         */
        public float getMaxRequiredWidth();

        /**
         * @return max match contents width for the suggestion.
         */
        public float getMaxMatchContentsWidth();
    }
}
