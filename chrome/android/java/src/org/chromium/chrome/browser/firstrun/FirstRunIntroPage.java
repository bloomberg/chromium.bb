// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.content.Context;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnLayoutChangeListener;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.ScrollView;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.util.FeatureUtilities;

/**
 * A page introducing a Chrome feature in the First Run Experience.
 */
public abstract class FirstRunIntroPage extends FirstRunPage {
    // SharedPreferences preference name to keep a bitmap of already visited pages.
    private static final String FIRST_RUN_INTRO_PAGE_VISITED_FLAGS = "first_run_intro_pages_bitmap";

    // The bitmasks for intro pages in the FRE.
    public static final long INTRO_RECENTS = 1L << 1;

    public static final long INTRO_ALL_NECESSARY_TABBED = 0L;
    public static final long INTRO_ALL_NECESSARY_DOCUMENT =
            INTRO_ALL_NECESSARY_TABBED | INTRO_RECENTS;

    /** @return Drawable resource id for the page image. */
    protected abstract int getIntroDrawableId();

    /** @return String resource id for the page title. */
    protected abstract int getIntroTitleId();

    /** @return String resource id for the page text. */
    protected abstract int getIntroTextId();

    private int mScrollViewHeight;

    // Fragment:
    @Override
    public View onCreateView(final LayoutInflater inflater, ViewGroup container,
            Bundle savedInstanceState) {
        ViewGroup rootView = (ViewGroup) inflater.inflate(R.layout.fre_page, container, false);

        rootView.findViewById(R.id.scroll_view).addOnLayoutChangeListener(
                new OnLayoutChangeListener() {
                    @Override
                    public void onLayoutChange(View v, int left, int top, int right, int bottom,
                            int oldLeft, int oldTop, int oldRight, int oldBottom) {
                        int newHeight = v.getHeight();
                        if (mScrollViewHeight != newHeight) {
                            mScrollViewHeight = newHeight;
                            if (v instanceof ScrollView) ((ScrollView) v).scrollTo(0, newHeight);
                        }
                    }
                });

        Button positiveButton = (Button) rootView.findViewById(R.id.positive_button);
        if (isLastPage()) positiveButton.setText(R.string.fre_done);
        positiveButton.setOnClickListener(
                new OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        // Completes FRE if this is the last page.
                        advanceToNextPage();
                    }
                });

        ((Button) rootView.findViewById(R.id.negative_button)).setOnClickListener(
                new OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        // Completes FRE if all next pages are intro.
                        getPageDelegate().skipIntroPages();
                    }
                });

        ((ImageView) rootView.findViewById(R.id.image_view)).setImageResource(getIntroDrawableId());
        ((TextView) rootView.findViewById(R.id.title)).setText(getIntroTitleId());
        ((TextView) rootView.findViewById(R.id.text)).setText(getIntroTextId());
        return rootView;
    }

    /**
     * @param context A context
     * @return A bitmap of already visited pages.
     */
    public static long getAlreadyShownPagesBitmap(Context context) {
        return PreferenceManager.getDefaultSharedPreferences(context)
                .getLong(FIRST_RUN_INTRO_PAGE_VISITED_FLAGS, 0L);
    }

    /**
     * @return Whether given intro pages have already been shown.
     * @param context A context
     * @param bitmask A bitmask to test
     */
    public static boolean wasShown(Context context, final long bitmask) {
        return (getAlreadyShownPagesBitmap(context) & bitmask) == bitmask;
    }

    /**
     * Marks given intro pages as already shown.
     * @param context A context
     * @param bitmask A bitmask to set
     */
    public static void markAsShown(Context context, final long bitmask) {
        PreferenceManager.getDefaultSharedPreferences(context)
                .edit()
                .putLong(FIRST_RUN_INTRO_PAGE_VISITED_FLAGS,
                        getAlreadyShownPagesBitmap(context) | bitmask)
                .apply();
    }

    /**
     * @return Whether all available intro pages has already been shown.
     * @param context A context
     */
    public static boolean wereAllNecessaryPagesShown(Context context) {
        return wasShown(context, getAllNecessaryPages(context));
    }

    /**
     * @return Mask for all intro pages.
     * @param context A context
     */
    public static long getAllNecessaryPages(Context context) {
        return FeatureUtilities.isDocumentModeEligible(context)
                ? INTRO_ALL_NECESSARY_DOCUMENT : INTRO_ALL_NECESSARY_TABBED;
    }

    /**
     * @return Mask for all intro pages that we could currently show.
     * @param context A context
     */
    public static long getAllPresentablePages(Context context) {
        return getAllNecessaryPages(context);
    }
}
