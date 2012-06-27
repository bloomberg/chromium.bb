// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.Context;
import android.util.Log;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.ScaleGestureDetector;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ZoomButtonsController;

/**
 * The ZoomManager is responsible for maintaining the ContentView's current zoom
 * level state. It is also responsible for managing the on-screen zoom controls.
 */
class ZoomManager {
    private static final String TAG = "ContentViewZoom";

    private ContentView mContentView;
    private ZoomButtonsController mZoomButtonsController;

    private class ScaleGestureListener implements ScaleGestureDetector.OnScaleGestureListener {
        // Completely silence scaling events. Used in WebView when zoom support
        // is turned off.
        private boolean mPermanentlyIgnoreDetectorEvents = false;
        // Bypass events through the detector to maintain its state. Used when
        // renderes already handles the touch event.
        private boolean mTemporarilyIgnoreDetectorEvents = false;

        // Whether any pinch zoom event has been sent to native.
        private boolean mPinchEventSent;

        boolean getPermanentlyIgnoreDetectorEvents() {
            return mPermanentlyIgnoreDetectorEvents;
        }

        void setPermanentlyIgnoreDetectorEvents(boolean value) {
            // Note that returning false from onScaleBegin / onScale makes the
            // gesture detector not to emit further scaling notifications
            // related to this gesture. Thus, if detector events are enabled in
            // the middle of the gesture, we don't need to do anything.
            mPermanentlyIgnoreDetectorEvents = value;
        }

        void setTemporarilyIgnoreDetectorEvents(boolean value) {
            mTemporarilyIgnoreDetectorEvents = value;
        }

        @Override
        public boolean onScaleBegin(ScaleGestureDetector detector) {
            if (ignoreDetectorEvents()) return false;
            mPinchEventSent = false;
            mContentView.setIgnoreSingleTap(true);
            return true;
        }

        @Override
        public void onScaleEnd(ScaleGestureDetector detector) {
            if (!mPinchEventSent || !mContentView.isAlive()) return;
            mContentView.pinchEnd(detector.getEventTime());
            mPinchEventSent = false;
        }

        @Override
        public boolean onScale(ScaleGestureDetector detector) {
            if (ignoreDetectorEvents()) return false;
            // It is possible that pinchBegin() was never called when we reach here.
            // This happens when webkit handles the 2nd touch down event. That causes
            // ContentView to ignore the onScaleBegin() call. And if webkit does not
            // handle the touch move events afterwards, we will face a situation
            // that pinchBy() is called without any pinchBegin().
            // To solve this problem, we call pinchBegin() here if it is never called.
            if (!mPinchEventSent) {
                mContentView.pinchBegin(detector.getEventTime(),
                        (int) detector.getFocusX(), (int) detector.getFocusY());
                mPinchEventSent = true;
            }
            mContentView.pinchBy(
                    detector.getEventTime(), (int) detector.getFocusX(), (int) detector.getFocusY(),
                    detector.getScaleFactor());
            return true;
        }

        private boolean ignoreDetectorEvents() {
            return mPermanentlyIgnoreDetectorEvents ||
                    mTemporarilyIgnoreDetectorEvents ||
                    !mContentView.isAlive();
        }
    }

    private ScaleGestureDetector mMultiTouchDetector;
    private ScaleGestureListener mMultiTouchListener;

    ZoomManager(final Context context, ContentView contentView) {
        mContentView = contentView;
        mMultiTouchListener = new ScaleGestureListener();
        mMultiTouchDetector = new ScaleGestureDetector(context, mMultiTouchListener);
    }

    void invokeZoomPicker() {
        ZoomButtonsController zoomControls = getZoomControls();
        if (zoomControls != null && !zoomControls.isVisible()) {
            zoomControls.setVisible(true);
        }
    }

    boolean isMultiTouchZoomSupported() {
        return !mMultiTouchListener.getPermanentlyIgnoreDetectorEvents();
    }

    boolean isScaleGestureDetectionInProgress() {
        return isMultiTouchZoomSupported()
                && mMultiTouchDetector.isInProgress();
    }

    // Passes the touch event to ScaleGestureDetector so that its internal
    // state won't go wrong, but instructs the listener to ignore the result
    // of processing, if any.
    void passTouchEventThrough(MotionEvent event) {
        mMultiTouchListener.setTemporarilyIgnoreDetectorEvents(true);
        try {
            mMultiTouchDetector.onTouchEvent(event);
        } catch (Exception e) {
            Log.e(TAG, "ScaleGestureDetector got into a bad state!", e);
            assert(false);
        }
    }

    // Passes the touch event to ScaleGestureDetector so that its internal state
    // won't go wrong. ScaleGestureDetector needs two pointers in a MotionEvent
    // to recognize a zoom gesture.
    boolean processTouchEvent(MotionEvent event) {
        // TODO: Need to deal with multi-touch transition
        mMultiTouchListener.setTemporarilyIgnoreDetectorEvents(false);
        try {
            return mMultiTouchDetector.onTouchEvent(event);
        } catch (Exception e) {
            Log.e(TAG, "ScaleGestureDetector got into a bad state!", e);
            assert(false);
        }
        return false;
    }

    void updateMultiTouchSupport() {
        mMultiTouchListener.setPermanentlyIgnoreDetectorEvents(
            !mContentView.getContentSettings().supportsMultiTouchZoom());
    }

    private ZoomButtonsController getZoomControls() {
        if (mZoomButtonsController == null &&
            mContentView.getContentSettings().shouldDisplayZoomControls()) {
            mZoomButtonsController = new ZoomButtonsController(mContentView);
            mZoomButtonsController.setOnZoomListener(new ZoomListener());
            // ZoomButtonsController positions the buttons at the bottom, but in
            // the middle. Change their layout parameters so they appear on the
            // right.
            View controls = mZoomButtonsController.getZoomControls();
            ViewGroup.LayoutParams params = controls.getLayoutParams();
            if (params instanceof FrameLayout.LayoutParams) {
                ((FrameLayout.LayoutParams) params).gravity = Gravity.RIGHT;
            }
        }
        return mZoomButtonsController;
    }

    // This method is used in tests. It doesn't modify the state of zoom controls.
    View getZoomControlsViewForTest() {
        return mZoomButtonsController != null ? mZoomButtonsController.getZoomControls() : null;
    }

    void updateZoomControls() {
        if (mZoomButtonsController == null) return;
        boolean canZoomIn = mContentView.canZoomIn();
        boolean canZoomOut = mContentView.canZoomOut();
        if (!canZoomIn && !canZoomOut) {
            // Hide the zoom in and out buttons if the page cannot zoom
            mZoomButtonsController.getZoomControls().setVisibility(View.GONE);
        } else {
            // Set each one individually, as a page may be able to zoom in or out
            mZoomButtonsController.setZoomInEnabled(canZoomIn);
            mZoomButtonsController.setZoomOutEnabled(canZoomOut);
        }
    }

    private class ZoomListener implements ZoomButtonsController.OnZoomListener {
        @Override
        public void onVisibilityChanged(boolean visible) {
            if (visible) {
                // Bring back the hidden zoom controls.
                mZoomButtonsController.getZoomControls().setVisibility(View.VISIBLE);
                updateZoomControls();
            }
        }

        @Override
        public void onZoom(boolean zoomIn) {
            if (zoomIn) {
                mContentView.zoomIn();
            } else {
                mContentView.zoomOut();
            }
            // ContentView will call updateZoomControls after its current page scale
            // is got updated from the native code.
        }
    }
}
