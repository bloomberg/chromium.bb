// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

import android.annotation.TargetApi;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.ReceiverCallNotAllowedException;
import android.os.Build;
import android.text.SpannableString;
import android.text.style.LocaleSpan;
import android.util.SparseArray;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.content.browser.RenderCoordinates;
import org.chromium.content_public.browser.WebContents;

import java.util.Locale;

/**
 * Subclass of WebContentsAccessibility for Lollipop.
 */
@JNINamespace("content")
@TargetApi(Build.VERSION_CODES.LOLLIPOP)
public class LollipopWebContentsAccessibility extends KitKatWebContentsAccessibility {
    private static SparseArray<AccessibilityAction> sAccessibilityActionMap =
            new SparseArray<AccessibilityAction>();
    private String mSystemLanguageTag;

    LollipopWebContentsAccessibility(Context context, ViewGroup containerView,
            WebContents webContents, RenderCoordinates renderCoordinates,
            boolean shouldFocusOnPageLoad) {
        super(context, containerView, webContents, renderCoordinates, shouldFocusOnPageLoad);

        // Cache the system language and set up a listener for when it changes.
        try {
            IntentFilter filter = new IntentFilter(Intent.ACTION_LOCALE_CHANGED);
            context.registerReceiver(new BroadcastReceiver() {
                @Override
                public void onReceive(Context context, Intent intent) {
                    mSystemLanguageTag = Locale.getDefault().toLanguageTag();
                }
            }, filter);
        } catch (ReceiverCallNotAllowedException e) {
            // WebView may be running inside a BroadcastReceiver, in which case registerReceiver is
            // not allowed.
        }
        mSystemLanguageTag = Locale.getDefault().toLanguageTag();
    }

    @Override
    protected void setAccessibilityNodeInfoLollipopAttributes(AccessibilityNodeInfo node,
            boolean canOpenPopup, boolean contentInvalid, boolean dismissable, boolean multiLine,
            int inputType, int liveRegion) {
        node.setCanOpenPopup(canOpenPopup);
        node.setContentInvalid(contentInvalid);
        node.setDismissable(contentInvalid);
        node.setMultiLine(multiLine);
        node.setInputType(inputType);
        node.setLiveRegion(liveRegion);
    }

    @Override
    protected void setAccessibilityNodeInfoCollectionInfo(
            AccessibilityNodeInfo node, int rowCount, int columnCount, boolean hierarchical) {
        node.setCollectionInfo(
                AccessibilityNodeInfo.CollectionInfo.obtain(rowCount, columnCount, hierarchical));
    }

    @Override
    protected void setAccessibilityNodeInfoCollectionItemInfo(AccessibilityNodeInfo node,
            int rowIndex, int rowSpan, int columnIndex, int columnSpan, boolean heading) {
        node.setCollectionItemInfo(AccessibilityNodeInfo.CollectionItemInfo.obtain(
                rowIndex, rowSpan, columnIndex, columnSpan, heading));
    }

    @Override
    protected void setAccessibilityNodeInfoRangeInfo(
            AccessibilityNodeInfo node, int rangeType, float min, float max, float current) {
        node.setRangeInfo(AccessibilityNodeInfo.RangeInfo.obtain(rangeType, min, max, current));
    }

    @Override
    protected void setAccessibilityNodeInfoViewIdResourceName(
            AccessibilityNodeInfo node, String viewIdResourceName) {
        node.setViewIdResourceName(viewIdResourceName);
    }

    @Override
    protected void setAccessibilityEventLollipopAttributes(AccessibilityEvent event,
            boolean canOpenPopup, boolean contentInvalid, boolean dismissable, boolean multiLine,
            int inputType, int liveRegion) {
        // This is just a fallback for pre-Lollipop systems.
        // Do nothing on Lollipop and higher.
    }

    @Override
    protected void setAccessibilityEventCollectionInfo(
            AccessibilityEvent event, int rowCount, int columnCount, boolean hierarchical) {
        // This is just a fallback for pre-Lollipop systems.
        // Do nothing on Lollipop and higher.
    }

    @Override
    protected void setAccessibilityEventHeadingFlag(AccessibilityEvent event, boolean heading) {
        // This is just a fallback for pre-Lollipop systems.
        // Do nothing on Lollipop and higher.
    }

    @Override
    protected void setAccessibilityEventCollectionItemInfo(
            AccessibilityEvent event, int rowIndex, int rowSpan, int columnIndex, int columnSpan) {
        // This is just a fallback for pre-Lollipop systems.
        // Do nothing on Lollipop and higher.
    }

    @Override
    protected void setAccessibilityEventRangeInfo(
            AccessibilityEvent event, int rangeType, float min, float max, float current) {
        // This is just a fallback for pre-Lollipop systems.
        // Do nothing on Lollipop and higher.
    }

    @Override
    protected void addAction(AccessibilityNodeInfo node, int actionId) {
        // The Lollipop SDK requires us to call AccessibilityNodeInfo.addAction with an
        // AccessibilityAction argument, but to simplify things and share more code with
        // the pre-L SDK, we just cache a set of AccessibilityActions mapped by their ID.
        AccessibilityAction action = sAccessibilityActionMap.get(actionId);
        if (action == null) {
            action = new AccessibilityAction(actionId, null);
            sAccessibilityActionMap.put(actionId, action);
        }
        node.addAction(action);
    }

    @Override
    protected CharSequence computeText(String text, boolean annotateAsLink, String language) {
        CharSequence charSequence = super.computeText(text, annotateAsLink, language);
        if (!language.isEmpty() && !language.equals(mSystemLanguageTag)) {
            SpannableString spannable;
            if (charSequence instanceof SpannableString) {
                spannable = (SpannableString) charSequence;
            } else {
                spannable = new SpannableString(charSequence);
            }
            Locale locale = Locale.forLanguageTag(language);
            spannable.setSpan(new LocaleSpan(locale), 0, spannable.length(), 0);
            return spannable;
        }
        return charSequence;
    }
}
