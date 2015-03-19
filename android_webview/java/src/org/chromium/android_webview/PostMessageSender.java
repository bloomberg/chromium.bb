// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import java.util.ArrayDeque;

/**
 * Sanity checks and sends post messages to web. Queues messages if necessary.
 */
public class PostMessageSender implements AwMessagePortService.MessageChannelObserver {

    /**
     * The interface for message handler.
     */
    public static interface PostMessageSenderDelegate {

        /*
         * Posts a message to the destination frame for real. The unique message port
         * id of any transferred port should be known at this time.
         */
        void postMessageToWeb(String frameName, String message, String targetOrigin,
                int[] sentPortIds);

        /*
         * Whether the post message sender is ready to post messages.
         */
        boolean isPostMessageSenderReady();

        /*
         * Informs that all messages are posted and message queue is empty.
         */
        void onPostMessageQueueEmpty();
    };

    // A struct to store Message parameters that are sent from App to Web.
    private static class PostMessageParams {
        public String frameName;
        public String message;
        public String targetOrigin;
        public AwMessagePort[] sentPorts;

        public PostMessageParams(String frameName, String message, String targetOrigin,
                AwMessagePort[] sentPorts) {
            this.frameName = frameName;
            this.message = message;
            this.targetOrigin = targetOrigin;
            this.sentPorts = sentPorts;
        }
    }

    private PostMessageSenderDelegate mDelegate;
    private AwMessagePortService mService;

    // If a message that is sent from android webview to web has a pending port (a port that
    // is not internally created yet), this message, and any following messages will be
    // queued until the transferred port becomes ready. This is necessary to prevent any
    // reordering.
    private ArrayDeque<PostMessageParams> mMessageQueue = new ArrayDeque<PostMessageParams>();

    public PostMessageSender(PostMessageSenderDelegate delegate, AwMessagePortService service) {
        mDelegate = delegate;
        mService = service;
    }

    // TODO(sgurun) in code review it was found this was implemented wrongly
    // as mMessageQueue.size() > 0. write a test to catch this.
    public boolean isMessageQueueEmpty() {
        return mMessageQueue.size() == 0;
    }

    // Return true if any sent port is pending.
    private boolean anySentPortIsPending(AwMessagePort[] sentPorts) {
        if (sentPorts != null) {
            for (AwMessagePort port : sentPorts) {
                if (!port.isReady()) {
                    return true;
                }
            }
        }
        return false;
    }

    // A message to a frame is queued if:
    // 1. Sender is not ready to post. When posting messages to frames, sender is always
    // ready. However, when posting messages using message channels, sender may be in
    // a pending state.
    // 2. There are already queued messages
    // 3. The message includes a port that is not ready yet.
    private boolean shouldQueueMessage(AwMessagePort[] sentPorts) {
        // if messages to frames are already in queue mode, simply queue it, no need to
        // check ports.
        if (mMessageQueue.size() > 0 || !mDelegate.isPostMessageSenderReady()) {
            return true;
        }
        if (anySentPortIsPending(sentPorts)) {
            return true;
        }
        return false;
    }

    private void postMessageToWeb(String frameName, String message, String targetOrigin,
            AwMessagePort[] sentPorts) {
        int[] portIds = null;
        if (sentPorts != null) {
            portIds = new int[sentPorts.length];
            for (int i = 0; i < sentPorts.length; i++) {
                portIds[i] = sentPorts[i].portId();
            }
            mService.removeSentPorts(portIds);
        }
        mDelegate.postMessageToWeb(frameName, message, targetOrigin, portIds);
    }

    /*
     * Sanity checks the message and queues it if necessary. Posts the message to delegate
     * when message can be sent.
     */
    public void postMessage(String frameName, String message, String targetOrigin,
            AwMessagePort[] sentPorts) throws IllegalStateException {
        // Sanity check all the ports that are being transferred.
        if (sentPorts != null) {
            for (AwMessagePort p : sentPorts) {
                if (p.isClosed()) {
                    throw new IllegalStateException("Closed port cannot be transfered");
                }
                if (p.isTransferred()) {
                    throw new IllegalStateException("Port cannot be re-transferred");
                }
                if (p.isStarted()) {
                    throw new IllegalStateException("Started port cannot be transferred");
                }
                p.setTransferred();
            }
        }
        if (shouldQueueMessage(sentPorts)) {
            mMessageQueue.add(new PostMessageParams(frameName, message, targetOrigin,
                    sentPorts));
        } else {
            postMessageToWeb(frameName, message, targetOrigin, sentPorts);
        }
    }

    /*
     * Starts posting any messages that are queued.
     */
    public void onMessageChannelCreated() {
        PostMessageParams msg;

        if (!mDelegate.isPostMessageSenderReady()) {
            return;
        }

        while ((msg = mMessageQueue.peek()) != null) {
            // If there are still pending ports, message cannot be posted.
            if (anySentPortIsPending(msg.sentPorts)) {
                return;
            }
            mMessageQueue.remove();
            postMessageToWeb(msg.frameName, msg.message, msg.targetOrigin, msg.sentPorts);
        }
        mDelegate.onPostMessageQueueEmpty();
    }
}
