// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.content.Context;
import android.util.Pair;
import android.util.SparseArray;
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
import org.chromium.chrome.browser.modelutil.PropertyModelChangeProcessor.ViewBinder;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionViewProperties.SuggestionIcon;

import java.util.ArrayList;
import java.util.List;

/**
 * Adapter for providing data and views to the omnibox results list.
 */
@VisibleForTesting
public class OmniboxResultsAdapter extends BaseAdapter {
    /**
     * An interface to provide a means to build specific view types.
     * @param <T> The type of view that the implementor will build.
     */
    public interface ViewBuilder<T extends View> {
        /**
         * @return A new view to show in the list.
         */
        T buildView();
    }

    private final Context mContext;
    private final List<Pair<Integer, PropertyModel>> mSuggestionItems = new ArrayList<>();
    private final SparseArray<Pair<ViewBuilder, ViewBinder>> mViewBuilderMap = new SparseArray<>();

    public OmniboxResultsAdapter(Context context) {
        mContext = context;
    }

    /**
     * Update the visible omnibox suggestions.
     */
    public void updateSuggestions(List<Pair<Integer, PropertyModel>> suggestionModels) {
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

    /**
     * Register a new view type that this adapter knows how to show.
     * @param typeId The ID of the view type. This should not match any other view type registered
     *               in this adapter.
     * @param builder A mechanism for building new views of the specified type.
     * @param binder A means of binding a model to the provided view.
     */
    public <T extends View> void registerType(
            int typeId, ViewBuilder<T> builder, ViewBinder<PropertyModel, T, PropertyKey> binder) {
        assert mViewBuilderMap.valueAt(typeId) == null;
        mViewBuilderMap.put(typeId, new Pair<>(builder, binder));
    }

    @Override
    public int getItemViewType(int position) {
        return mSuggestionItems.get(position).first;
    }

    @Override
    public int getViewTypeCount() {
        return Math.max(1, mViewBuilderMap.size());
    }

    @SuppressWarnings("unchecked")
    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
        if (convertView == null) {
            int suggestionTypeId = mSuggestionItems.get(position).first;
            convertView = mViewBuilderMap.get(suggestionTypeId).first.buildView();
        }

        PropertyModel suggestionModel = mSuggestionItems.get(position).second;
        PropertyModel viewModel =
                getOrCreateModelFromExisting(convertView, mSuggestionItems.get(position));
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
        convertView.jumpDrawablesToCurrentState();

        return convertView;
    }

    @SuppressWarnings("unchecked")
    private PropertyModel getOrCreateModelFromExisting(
            View view, Pair<Integer, PropertyModel> item) {
        PropertyModel model = (PropertyModel) view.getTag(R.id.view_model);
        if (model == null) {
            model = new PropertyModel(item.second.getAllProperties());
            PropertyModelChangeProcessor.create(
                    model, view, mViewBuilderMap.get(item.first).second);
            model.set(SuggestionViewProperties.SUGGESTION_ICON_TYPE, SuggestionIcon.UNDEFINED);
            view.setTag(R.id.view_model, model);
        }
        return model;
    }
}
