// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

import android.content.Context;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.widget.ImageButton;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.banners.SwipableOverlayView;
import org.chromium.content.browser.ContentViewCore;

/**
 * A view which displays a question to the user about the quality of distillation, where the user
 * is given the option to respond.
 *
 * <p>The observer is called when the user makes a choice. After this point, it is not possible to
 * interact with the view, and it is ready for dismissal. The selected option stays visibly
 * selected.
 */
public class DomDistillerFeedbackReportingView extends SwipableOverlayView {
    // XML layout for the BannerView.
    private static final int VIEW_LAYOUT = R.layout.dom_distiller_feedback_reporting_view;

    // Class to alert about DomDistillerFeedbackReportingView events.
    private FeedbackObserver mFeedbackObserver;

    // The button to click for selecting 'No'.
    private ImageButton mNoButton;

    // The button to click for selecting 'Yes'.
    private ImageButton mYesButton;

    // Whether a selection has already been made, which means new events should be ignored.
    private boolean mSelectionMade;

    /**
     * Called when the user makes a choice. After the call, it is not possible to interact further
     * with the view.
     */
    interface FeedbackObserver {
        void onYesPressed(DomDistillerFeedbackReportingView view);

        void onNoPressed(DomDistillerFeedbackReportingView view);
    }

    /**
     * Creates a DomDistillerFeedbackReportingView and adds it to the given ContentViewCore.
     *
     * @param contentView      ContentViewCore to display the DomDistillerFeedbackReportingView for.
     * @param feedbackObserver Class that is alerted for DomDistillerFeedbackReportingView events.
     * @return The created banner.
     */
    public static DomDistillerFeedbackReportingView create(ContentViewCore contentViewCore,
                                               FeedbackObserver feedbackObserver) {
        Context context = contentViewCore.getContext().getApplicationContext();
        DomDistillerFeedbackReportingView view =
                (DomDistillerFeedbackReportingView) LayoutInflater.from(context)
                        .inflate(VIEW_LAYOUT, null);
        view.initialize(feedbackObserver);
        view.addToView(contentViewCore);
        return view;
    }

    /**
     * Creates a DomDistillerFeedbackReportingView.
     *
     * @param context Context for acquiring resources.
     * @param attrs   Attributes from the XML layout inflation.
     */
    public DomDistillerFeedbackReportingView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    private void initialize(FeedbackObserver feedbackObserver) {
        mFeedbackObserver = feedbackObserver;
        mNoButton = (ImageButton) findViewById(R.id.distillation_quality_answer_no);
        mYesButton = (ImageButton) findViewById(R.id.distillation_quality_answer_yes);
        mNoButton.setClickable(true);
        mYesButton.setClickable(true);
        mNoButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                if (mSelectionMade) return;
                mSelectionMade = true;
                mNoButton.setImageResource(R.drawable.distillation_quality_answer_no_pressed);
                disableUI();
                if (mFeedbackObserver != null) {
                    mFeedbackObserver.onNoPressed(DomDistillerFeedbackReportingView.this);
                }
            }
        });
        mYesButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                if (mSelectionMade) return;
                mSelectionMade = true;
                mYesButton.setImageResource(R.drawable.distillation_quality_answer_yes_pressed);
                disableUI();
                if (mFeedbackObserver != null) {
                    mFeedbackObserver.onYesPressed(DomDistillerFeedbackReportingView.this);
                }
            }
        });
    }

    private void disableUI() {
        // Clear OnClickListener to assure no more calls and that everything is cleaned up.
        mNoButton.setOnClickListener(null);
        mYesButton.setOnClickListener(null);

        // Disable the buttons, so the images for highlighted/non-highlighted will not change if the
        // user continues to tap the buttons while it is dismissing.
        mNoButton.setEnabled(false);
        mYesButton.setEnabled(false);
    }

    /**
     * This is overridden since the method visibility is protected in the parent
     * {@link SwipableOverlayView}. The {@link DomDistillerFeedbackReporter} needs to be able to
     * dismiss this {@link DomDistillerFeedbackReportingView}, so by overriding this method in this
     * class, it is callable from {@link DomDistillerFeedbackReporter}.
     */
    @Override
    protected boolean dismiss(boolean horizontally) {
        return super.dismiss(horizontally);
    }

    @Override
    protected void onViewClicked() {
    }

    @Override
    protected void onViewPressed(MotionEvent event) {
    }

    @Override
    protected void onViewSwipedAway() {
    }
}
