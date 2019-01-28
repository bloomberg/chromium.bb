// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.overlay;

import android.graphics.RectF;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/**
 * State for the header of the Autofill Assistant.
 */
@JNINamespace("autofill_assistant")
public class AssistantOverlayModel extends PropertyModel {
    public static final WritableObjectPropertyKey<AssistantOverlayState> STATE =
            new WritableObjectPropertyKey<>();

    public AssistantOverlayModel() {
        super(STATE);
    }

    @CalledByNative
    private void setHidden() {
        set(STATE, AssistantOverlayState.hidden());
    }

    @CalledByNative
    private void setFull() {
        set(STATE, AssistantOverlayState.full());
    }

    @CalledByNative
    private void setPartial(float[] coords) {
        List<RectF> boxes = new ArrayList<>();
        for (int i = 0; i < coords.length; i += 4) {
            boxes.add(new RectF(/* left= */ coords[i], /* top= */ coords[i + 1],
                    /* right= */ coords[i + 2], /* bottom= */ coords[i + 3]));
        }
        set(STATE, AssistantOverlayState.partial(boxes));
    }
}
