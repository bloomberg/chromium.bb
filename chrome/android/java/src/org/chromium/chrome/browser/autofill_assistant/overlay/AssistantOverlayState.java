// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.overlay;

import android.graphics.RectF;

import java.util.Collections;
import java.util.List;

/**
 * A state class for the overlay.
 */
public abstract class AssistantOverlayState {
    private AssistantOverlayState() {}

    boolean isHidden() {
        return this instanceof Hidden;
    }

    boolean isFull() {
        return this instanceof Full;
    }

    boolean isPartial() {
        return this instanceof Partial;
    }

    List<RectF> boxes() {
        if (this instanceof Partial) {
            return ((Partial) this).mBoxes;
        }
        return Collections.emptyList();
    }

    /**
     * Return an AssistantOverlayState representing a hidden overlay.
     */
    public static AssistantOverlayState hidden() {
        return Hidden.INSTANCE;
    }

    /**
     * Return an AssistantOverlayState representing a full overlay.
     */
    public static AssistantOverlayState full() {
        return Full.INSTANCE;
    }

    /**
     * Return an AssistantOverlayState representing a partial overlay.
     */
    public static AssistantOverlayState partial(List<RectF> boxes) {
        return new Partial(boxes);
    }

    private static class Hidden extends AssistantOverlayState {
        private static final AssistantOverlayState INSTANCE = new Hidden();

        // Default equals method is correct as there can be only one instance of
        // AssistantOverlayState.Hidden.
    }

    private static class Full extends AssistantOverlayState {
        private static final AssistantOverlayState INSTANCE = new Full();

        // Default equals method is correct as there can be only one instance of
        // AssistantOverlayState.Full.
    }

    private static class Partial extends AssistantOverlayState {
        private final List<RectF> mBoxes;

        private Partial(List<RectF> boxes /*, float[] coords*/) {
            // this.coords = coords;
            mBoxes = boxes;
        }

        @Override
        public boolean equals(Object o) {
            if (this == o) {
                return true;
            }
            if (o == null || getClass() != o.getClass()) {
                return false;
            }

            Partial partial = (Partial) o;

            return mBoxes.equals(partial.mBoxes);
        }
    }
}
