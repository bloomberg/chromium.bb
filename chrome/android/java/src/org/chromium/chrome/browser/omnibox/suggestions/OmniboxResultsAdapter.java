// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.modelutil.PropertyKey;
import org.chromium.chrome.browser.modelutil.PropertyModel;
import org.chromium.chrome.browser.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.chrome.browser.modelutil.PropertyModel.WritableFloatPropertyKey;
import org.chromium.chrome.browser.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.chrome.browser.modelutil.PropertyModel.WritableObjectPropertyKey;
import org.chromium.chrome.browser.modelutil.PropertyModelChangeProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionViewProperties.SuggestionIcon;

import java.util.ArrayList;
import java.util.List;

/**
 * Adapter for providing data and views to the omnibox results list.
 */
@VisibleForTesting
public class OmniboxResultsAdapter extends BaseAdapter {
    private final Context mContext;
    private final List<PropertyModel> mSuggestionItems = new ArrayList<>();

    public OmniboxResultsAdapter(Context context) {
        mContext = context;
    }

    /**
     * Update the visible omnibox suggestions.
     */
    public void updateSuggestions(List<PropertyModel> suggestionModels) {
        mSuggestionItems.clear();
        mSuggestionItems.addAll(suggestionModels);
        notifyDataSetChanged();
    }

    @Override
    public int getCount() {
        return mSuggestionItems.size();
    }

    @Override
    public Object getItem(int position) {
        return mSuggestionItems.get(position);
    }

    @Override
    public long getItemId(int position) {
        return position;
    }

    @SuppressWarnings("unchecked")
    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
        SuggestionView suggestionView;
        if (convertView instanceof SuggestionView) {
            suggestionView = (SuggestionView) convertView;
        } else {
            suggestionView = new SuggestionView(mContext);
        }
        PropertyModel suggestionModel = mSuggestionItems.get(position);
        PropertyModel viewModel = getModel(suggestionView);
        for (PropertyKey key : suggestionModel.getAllSetProperties()) {
            if (key instanceof WritableIntPropertyKey) {
                WritableIntPropertyKey intKey = (WritableIntPropertyKey) key;
                viewModel.set(intKey, suggestionModel.get(intKey));
            } else if (key instanceof WritableBooleanPropertyKey) {
                WritableBooleanPropertyKey booleanKey = (WritableBooleanPropertyKey) key;
                viewModel.set(booleanKey, suggestionModel.get(booleanKey));
            } else if (key instanceof WritableFloatPropertyKey) {
                WritableFloatPropertyKey floatKey = (WritableFloatPropertyKey) key;
                viewModel.set(floatKey, suggestionModel.get(floatKey));
            } else if (key instanceof WritableObjectPropertyKey<?>) {
                @SuppressWarnings({"unchecked", "rawtypes"})
                WritableObjectPropertyKey objectKey = (WritableObjectPropertyKey) key;
                viewModel.set(objectKey, suggestionModel.get(objectKey));
            } else {
                assert false : "Unexpected key received";
            }
        }
        // TODO(tedchoc): Investigate whether this is still needed.
        suggestionView.jumpDrawablesToCurrentState();

        return suggestionView;
    }

    private PropertyModel getModel(SuggestionView view) {
        PropertyModel model = (PropertyModel) view.getTag(R.id.view_model);
        if (model == null) {
            model = new PropertyModel(SuggestionViewProperties.ALL_KEYS);
            PropertyModelChangeProcessor.create(model, view, SuggestionViewViewBinder::bind);
            model.set(SuggestionViewProperties.SUGGESTION_ICON_TYPE, SuggestionIcon.UNDEFINED);
            view.setTag(R.id.view_model, model);
        }
        return model;
    }
}
