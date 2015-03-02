// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

import android.annotation.TargetApi;
import android.content.Context;
import android.os.Build;
import android.os.Bundle;
import android.view.View;
import android.view.accessibility.AccessibilityNodeInfo;

import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.JavascriptInterface;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.Iterator;

/**
 * Handles injecting accessibility Javascript and related Javascript -> Java APIs for Lollipop and
 * newer devices.
 */
@TargetApi(Build.VERSION_CODES.LOLLIPOP)
class LollipopAccessibilityInjector extends JellyBeanAccessibilityInjector {
    /**
     * Constructs an instance of the LollipopAccessibilityInjector.
     * @param view The ContentViewCore that this AccessibilityInjector manages.
     */
    protected LollipopAccessibilityInjector(ContentViewCore view) {
        super(view);
    }

    @Override
    public void onInitializeAccessibilityNodeInfo(AccessibilityNodeInfo info) {
        info.setMovementGranularities(
                AccessibilityNodeInfo.MOVEMENT_GRANULARITY_CHARACTER
                | AccessibilityNodeInfo.MOVEMENT_GRANULARITY_WORD
                | AccessibilityNodeInfo.MOVEMENT_GRANULARITY_LINE
                | AccessibilityNodeInfo.MOVEMENT_GRANULARITY_PARAGRAPH
                | AccessibilityNodeInfo.MOVEMENT_GRANULARITY_PAGE);
        info.addAction(
                AccessibilityNodeInfo.AccessibilityAction.ACTION_NEXT_AT_MOVEMENT_GRANULARITY);
        info.addAction(
                AccessibilityNodeInfo.AccessibilityAction.ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY);
        info.addAction(AccessibilityNodeInfo.AccessibilityAction.ACTION_NEXT_HTML_ELEMENT);
        info.addAction(AccessibilityNodeInfo.AccessibilityAction.ACTION_PREVIOUS_HTML_ELEMENT);
        info.addAction(AccessibilityNodeInfo.AccessibilityAction.ACTION_CLICK);
        info.setClickable(true);
    }

    @Override
    protected TextToSpeechWrapper createTextToSpeechWrapper(View view, Context context) {
        return new LTextToSpeechWrapper(view, context);
    }

    protected static class LTextToSpeechWrapper extends AccessibilityInjector.TextToSpeechWrapper {
        private LTextToSpeechWrapper(View view, Context context) {
            super(view, context);
        }

        @Override
        @JavascriptInterface
        @SuppressWarnings("unused")
        public int speak(String text, int queueMode, String jsonParams) {
            // Try to pull the params from the JSON string.
            Bundle bundle = null;
            try {
                if (jsonParams != null) {
                    bundle = new Bundle();
                    JSONObject json = new JSONObject(jsonParams);

                    // Using legacy API here.
                    @SuppressWarnings("unchecked")
                    Iterator<String> keyIt = json.keys();

                    while (keyIt.hasNext()) {
                        String key = keyIt.next();
                        // Only add parameters that are raw data types.
                        if (json.optJSONObject(key) == null && json.optJSONArray(key) == null) {
                            bundle.putCharSequence(key, json.getString(key));
                        }
                    }
                }
            } catch (JSONException e) {
                bundle = null;
            }

            return mTextToSpeech.speak(text, queueMode, bundle, null);
        }
    }
}
