// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar.contextualsearch;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.ViewGroup;
import android.view.ViewTreeObserver.OnDrawListener;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.ui.resources.dynamics.ViewResourceAdapter;


/**
 * Root ControlContainer for the Contextual Search panel.
 * Handles user interaction with the bottom bar text.
 * Based on ToolbarControlContainer.
 */
public class BottomBarTextControl {
    /**
     * Object Replacement Character that is used in place of HTML objects that cannot be represented
     * as text (e.g. images). Contextual search panel should not be displaying such characters as
     * they get shown as [obj] character.
     */
    private static final String OBJ_CHARACTER = "\uFFFC";

    private Context mContext;
    private ViewGroup mContainer;

    private ViewGroup mSearchContextView;
    private TextView mSelectedText;
    private TextView mStartText;
    private TextView mEndText;
    private ViewResourceAdapter mSearchContextResourceAdapter;
    private boolean mIsSearchContextDirty;

    private ViewGroup mSearchTermView;
    private TextView mSearchTermText;
    private ViewResourceAdapter mSearchTermResourceAdapter;
    private boolean mIsSearchTermDirty;

    /**
     * Constructs a new bottom bar control container by inflating views from XML.
     *
     * @param context The context used to build this view.
     * @param container The parent view for the bottom bar views.
     */
    public BottomBarTextControl(Context context, ViewGroup container) {
        mContext = context;
        mContainer = container;

        // TODO(pedrosimonetti): For now, we're still relying on a Android View
        // to render the text that appears in the Search Bar. The View will be
        // invisible and will not capture events. Consider rendering the text
        // in the Compositor and get rid of the View entirely.
        // TODO(twellington): Make this class more generic so that it can be reused by ReaderMode.
        LayoutInflater.from(mContext).inflate(R.layout.contextual_search_term_view, container);
        mSearchTermView = (ViewGroup) container.findViewById(R.id.contextual_search_term_view);
        mSearchTermText = (TextView) container.findViewById(R.id.contextual_search_term);
        mSearchTermResourceAdapter = new ViewResourceAdapter(mSearchTermView);

        // mSearchTermResourceAdapter needs to be invalidated in OnDraw after it has been measured.
        // Without this OnDrawListener, the search text view wasn't always appearing on the panel
        // for RTL text.
        mSearchTermView.getViewTreeObserver().addOnDrawListener(new OnDrawListener() {
            @Override
            public void onDraw() {
                if (mIsSearchTermDirty) {
                    mSearchTermResourceAdapter.invalidate(null);
                    mIsSearchTermDirty = false;
                }
            }
        });

        LayoutInflater.from(mContext).inflate(R.layout.contextual_search_context_view, container);
        mSearchContextView = (ViewGroup) container.findViewById(
                R.id.contextual_search_context_view);
        mSelectedText = (TextView) mSearchContextView.findViewById(R.id.selected_text);
        mStartText = (TextView) mSearchContextView.findViewById(R.id.surrounding_text_start);
        mEndText = (TextView) mSearchContextView.findViewById(R.id.surrounding_text_end);
        mSearchContextResourceAdapter = new ViewResourceAdapter(mSearchContextView);

        // mSearchContextResourceAdapter needs to be invalidated in OnDraw after it has been
        // measured. Without this OnDrawListener, the selected text view was occasionally getting
        // ellipsized.
        mSearchContextView.getViewTreeObserver().addOnDrawListener(new OnDrawListener() {
            @Override
            public void onDraw() {
                if (mIsSearchContextDirty) {
                    mSearchContextResourceAdapter.invalidate(null);
                    mIsSearchContextDirty = false;
                }
            }
        });
    }

    /**
     * @return The {@link ViewResourceAdapter} that exposes the search context view as a CC
     *         resource.
     */
    public ViewResourceAdapter getSearchContextResourceAdapter() {
        return mSearchContextResourceAdapter;
    }

    /**
     * @return The {@link ViewResourceAdapter} that exposes the search term view as a CC resource.
     */
    public ViewResourceAdapter getSearchTermResourceAdapter() {
        return mSearchTermResourceAdapter;
    }

    /**
     * @param width The width of the container.
     */
    public void setWidth(int width) {
        mSearchContextView.getLayoutParams().width = width;
        mSearchContextView.requestLayout();
        mSearchTermView.getLayoutParams().width = width;
        mSearchTermView.requestLayout();
    }

    /**
     * Removes the bottom bar views from the parent container.
     */
    public void removeFromContainer() {
        mContainer.removeView(mSearchTermView);
        mContainer.removeView(mSearchContextView);
    }

    /**
     * Sets the search context to display in the control.
     * @param selection The portion of the context that represents the user's selection.
     * @param start The portion of the context from its start to the selection.
     * @param end The portion of the context the selection to its end.
     */
    public void setSearchContext(String selection, String start, String end) {
        mSelectedText.setText(sanitizeText(selection));
        mStartText.setText(sanitizeText(start));
        mEndText.setText(sanitizeText(end));
        mIsSearchContextDirty = true;
    }

    /**
     * Sets the search term to display in the control.
     * @param searchTerm The string that represents the search term.
     */
    public void setSearchTerm(String searchTerm) {
        mSearchTermText.setText(searchTerm);
        mIsSearchTermDirty = true;
    }

    private String sanitizeText(String text) {
        if (text == null) return null;
        return text.replace(OBJ_CHARACTER, " ");
    }
}
