// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

import android.annotation.TargetApi;
import android.os.Build;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.content.browser.ContentViewCore;

/**
 * Subclass of BrowserAccessibilityManager for Lollipop.
 */
@JNINamespace("content")
@TargetApi(Build.VERSION_CODES.LOLLIPOP)
public class LollipopBrowserAccessibilityManager extends KitKatBrowserAccessibilityManager {
    LollipopBrowserAccessibilityManager(long nativeBrowserAccessibilityManagerAndroid,
            ContentViewCore contentViewCore) {
        super(nativeBrowserAccessibilityManagerAndroid, contentViewCore);
    }

    @Override
    protected void setAccessibilityNodeInfoLollipopAttributes(AccessibilityNodeInfo node,
            boolean canOpenPopup,
            boolean contentInvalid,
            boolean dismissable,
            boolean multiLine,
            int inputType,
            int liveRegion) {
        node.setCanOpenPopup(canOpenPopup);
        node.setContentInvalid(contentInvalid);
        node.setDismissable(contentInvalid);
        node.setMultiLine(multiLine);
        node.setInputType(inputType);
        node.setLiveRegion(liveRegion);
    }

    @Override
    protected void setAccessibilityNodeInfoCollectionInfo(AccessibilityNodeInfo node,
            int rowCount, int columnCount, boolean hierarchical) {
        node.setCollectionInfo(AccessibilityNodeInfo.CollectionInfo.obtain(
                rowCount, columnCount, hierarchical));
    }

    @Override
    protected void setAccessibilityNodeInfoCollectionItemInfo(AccessibilityNodeInfo node,
            int rowIndex, int rowSpan, int columnIndex, int columnSpan, boolean heading) {
        node.setCollectionItemInfo(AccessibilityNodeInfo.CollectionItemInfo.obtain(
                rowIndex, rowSpan, columnIndex, columnSpan, heading));
    }

    @Override
    protected void setAccessibilityNodeInfoRangeInfo(AccessibilityNodeInfo node,
            int rangeType, float min, float max, float current) {
        node.setRangeInfo(AccessibilityNodeInfo.RangeInfo.obtain(
                rangeType, min, max, current));
    }

    @Override
    protected void setAccessibilityNodeInfoViewIdResourceName(
            AccessibilityNodeInfo node, String viewIdResourceName) {
        node.setViewIdResourceName(viewIdResourceName);
    }

    @Override
    protected void setAccessibilityEventLollipopAttributes(AccessibilityEvent event,
            boolean canOpenPopup,
            boolean contentInvalid,
            boolean dismissable,
            boolean multiLine,
            int inputType,
            int liveRegion) {
        // This is just a fallback for pre-Lollipop systems.
        // Do nothing on Lollipop and higher.
    }

    @Override
    protected void setAccessibilityEventCollectionInfo(AccessibilityEvent event,
            int rowCount, int columnCount, boolean hierarchical) {
        // This is just a fallback for pre-Lollipop systems.
        // Do nothing on Lollipop and higher.
    }

    @Override
    protected void setAccessibilityEventHeadingFlag(AccessibilityEvent event,
            boolean heading) {
        // This is just a fallback for pre-Lollipop systems.
        // Do nothing on Lollipop and higher.
    }

    @Override
    protected void setAccessibilityEventCollectionItemInfo(AccessibilityEvent event,
            int rowIndex, int rowSpan, int columnIndex, int columnSpan) {
        // This is just a fallback for pre-Lollipop systems.
        // Do nothing on Lollipop and higher.
    }

    @Override
    protected void setAccessibilityEventRangeInfo(AccessibilityEvent event,
            int rangeType, float min, float max, float current) {
        // This is just a fallback for pre-Lollipop systems.
        // Do nothing on Lollipop and higher.
    }

    @CalledByNative
    @Override
    protected void addAccessibilityNodeInfoActions(AccessibilityNodeInfo node,
            int virtualViewId, boolean canScrollForward, boolean canScrollBackward,
            boolean canScrollUp, boolean canScrollDown, boolean canScrollLeft,
            boolean canScrollRight, boolean clickable, boolean editableText, boolean enabled,
            boolean focusable, boolean focused) {
        node.addAction(AccessibilityAction.ACTION_NEXT_HTML_ELEMENT);
        node.addAction(AccessibilityAction.ACTION_PREVIOUS_HTML_ELEMENT);
        node.addAction(AccessibilityAction.ACTION_NEXT_AT_MOVEMENT_GRANULARITY);
        node.addAction(AccessibilityAction.ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY);

        if (editableText && enabled) {
            node.addAction(AccessibilityAction.ACTION_SET_TEXT);
            node.addAction(AccessibilityAction.ACTION_SET_SELECTION);
        }

        if (canScrollForward) {
            node.addAction(AccessibilityAction.ACTION_SCROLL_FORWARD);
        }

        if (canScrollBackward) {
            node.addAction(AccessibilityAction.ACTION_SCROLL_BACKWARD);
        }

        // TODO(dmazzoni): add custom actions for scrolling up, down,
        // left, and right.

        if (focusable) {
            if (focused) {
                node.addAction(AccessibilityAction.ACTION_CLEAR_FOCUS);
            } else {
                node.addAction(AccessibilityAction.ACTION_FOCUS);
            }
        }

        if (mAccessibilityFocusId == virtualViewId) {
            node.addAction(AccessibilityAction.ACTION_CLEAR_ACCESSIBILITY_FOCUS);
        } else {
            node.addAction(AccessibilityAction.ACTION_ACCESSIBILITY_FOCUS);
        }

        if (clickable) {
            node.addAction(AccessibilityAction.ACTION_CLICK);
        }
    }
}
