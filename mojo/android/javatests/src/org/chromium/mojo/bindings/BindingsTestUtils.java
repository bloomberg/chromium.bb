// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.bindings;

import org.chromium.mojo.system.MojoException;

import java.util.ArrayList;
import java.util.List;

/**
 * Utility class for bindings tests.
 */
public class BindingsTestUtils {

    /**
     * {@link MessageReceiver} that records any message it receives.
     */
    public static class RecordingMessageReceiver implements MessageReceiver {

        public final List<Message> messages = new ArrayList<Message>();

        /**
         * @see MessageReceiver#accept(Message)
         */
        @Override
        public boolean accept(Message message) {
            messages.add(message);
            return true;
        }
    }

    /**
     * {@link Connector.ErrorHandler} that records any error it received.
     */
    public static class CapturingErrorHandler implements Connector.ErrorHandler {

        public MojoException exception = null;

        /**
         * @see Connector.ErrorHandler#onError(MojoException)
         */
        @Override
        public void onError(MojoException e) {
            exception = e;
        }
    }

}
