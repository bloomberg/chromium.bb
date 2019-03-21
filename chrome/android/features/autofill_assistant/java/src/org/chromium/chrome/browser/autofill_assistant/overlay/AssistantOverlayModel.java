// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.overlay;

import android.graphics.RectF;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/**
 * State for the header of the Autofill Assistant.
 */
@JNINamespace("autofill_assistant")
public class AssistantOverlayModel extends PropertyModel {
    public static final WritableIntPropertyKey STATE = new WritableIntPropertyKey();

    // Skipping equality as a way of fixing offset issues. See b/129048184.
    // TODO(b/129050125): Handle offsets properly and remove.
    public static final WritableObjectPropertyKey<List<RectF>> TOUCHABLE_AREA =
            new WritableObjectPropertyKey<>(/* skipEquality= */ true);

    public static final WritableObjectPropertyKey<AssistantOverlayDelegate> DELEGATE =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<WebContents> WEB_CONTENTS =
            new WritableObjectPropertyKey<>();

    public AssistantOverlayModel() {
        super(STATE, TOUCHABLE_AREA, DELEGATE, WEB_CONTENTS);
    }

    @CalledByNative
    private void setState(@AssistantOverlayState int state) {
        set(STATE, state);
    }

    @CalledByNative
    private void setTouchableArea(float[] coords) {
        List<RectF> boxes = new ArrayList<>();
        for (int i = 0; i < coords.length; i += 4) {
            boxes.add(new RectF(/* left= */ coords[i], /* top= */ coords[i + 1],
                    /* right= */ coords[i + 2], /* bottom= */ coords[i + 3]));
        }
        set(TOUCHABLE_AREA, boxes);
    }

    @CalledByNative
    private void setDelegate(AssistantOverlayDelegate delegate) {
        set(DELEGATE, delegate);
    }

    @CalledByNative
    private void setWebContents(WebContents webContents) {
        set(WEB_CONTENTS, webContents);
    }
}
