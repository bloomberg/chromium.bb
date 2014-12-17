// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge;

import android.content.Intent;
import android.content.SharedPreferences;
import android.util.Log;

import org.chromium.components.devtools_bridge.apiary.ApiaryClientFactory;
import org.chromium.components.devtools_bridge.apiary.OAuthResult;
import org.chromium.components.devtools_bridge.commands.Command;
import org.chromium.components.devtools_bridge.commands.CommandReceiver;
import org.chromium.components.devtools_bridge.gcd.InstanceCredential;
import org.chromium.components.devtools_bridge.gcd.Notification;

import java.io.IOException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/**
 * Handles notifications from GCD. For command notification it delegates work to
 * DevToolsBridgeServer and updates command state when it finish. for unregistration
 * notification it updates preferences.
 */
public class GCDNotificationHandler {
    private static final String TAG = "GCDNotificationHandler";

    private static final String EXTRA_FROM = "from";
    private static final String EXTRA_NOTIFICATION = "notification";
    private static final String EXPECTED_SENDER =
            "clouddevices-gcm@clouddevices.google.com";

    private final DevToolsBridgeServer mServer;
    private final ApiaryClientFactory mClientFactory;
    private final CommandReceiver mCommandReceiver;

    private final ExecutorService mIOExecutor;
    private OAuthResult mOAuthResult;

    public GCDNotificationHandler(DevToolsBridgeServer server) {
        mServer = server;
        mClientFactory = new ApiaryClientFactory();
        mCommandReceiver = new CommandReceiver(server);
        mIOExecutor = Executors.newSingleThreadExecutor();
    }

    public void dispose() {
        mIOExecutor.shutdown();
        mClientFactory.close();
    }

    public boolean isNotification(Intent intent) {
        return EXPECTED_SENDER.equals(intent.getStringExtra(EXTRA_FROM))
                && intent.getStringExtra(EXTRA_NOTIFICATION) != null;
    }

    public void onNotification(Intent intent, Runnable completionHandler) {
        try {
            handle(Notification.read(intent.getStringExtra(EXTRA_NOTIFICATION)), completionHandler);
            return;
        } catch (Notification.FormatException e) {
            Log.e(TAG, "Invalid notification", e);
        }
        completionHandler.run();
    }

    public void updateCloudMessagesId(final String gcmChannelId, final Runnable completionHandler) {
        final InstanceCredential credential = InstanceCredential.get(mServer.getPreferences());
        if (credential == null) return;

        mIOExecutor.submit(new Runnable() {
            @Override
            public void run() {
                try {
                    mClientFactory.newGCDClient(getAccessToken(credential))
                            .patchInstanceGCMChannel(credential.id, gcmChannelId);
                } catch (Exception e) {
                    Log.e(TAG, "Failure when updating GCM channel id", e);
                } finally {
                    completionHandler.run();
                }
            }
        });
    }

    private void handle(Notification notification, Runnable completionHandler) {
        if (notification == null) {
            // Unsupported notification type. Ignore.
            Log.i(TAG, "Unsupported notification");
            completionHandler.run();
            return;
        }

        switch (notification.type) {
            case INSTANCE_UNREGISTERED:
                onInstanceUnregistered(notification.instanceId, completionHandler);
                break;

            case COMMAND_CREATED:
                onCommand(notification.instanceId, notification.command, completionHandler);
                break;

            default:
                completionHandler.run();
                break;
        }
    }

    private void onInstanceUnregistered(String instanceId, Runnable completionHandler) {
        Log.i(TAG, "Received unregistration notification: " + instanceId);
        InstanceCredential credential = InstanceCredential.get(mServer.getPreferences());
        if (credential != null && credential.id.equals(instanceId)) {
            SharedPreferences.Editor editor = mServer.getPreferences().edit();
            InstanceCredential.remove(editor);
            editor.commit();
        }
        completionHandler.run();
    }

    private void onCommand(String instanceId, Command command, Runnable completionHandler) {
        InstanceCredential credential = InstanceCredential.get(mServer.getPreferences());
        if (credential != null && credential.id.equals(instanceId)) {
            mCommandReceiver.receive(
                    command, new Responder(command, credential, completionHandler));
        } else {
            Log.w(TAG, "Ignored command " + command.type + " for " + instanceId);
            completionHandler.run();
        }
    }

    private String getAccessToken(InstanceCredential credential) throws IOException {
        // Called on IO executor.
        // TODO(serya): mOAuthResult should be persistent.
        if (mOAuthResult == null) {
            mOAuthResult = mClientFactory.newOAuthClient().authenticate(
                    credential.secret);
        }
        return mOAuthResult.accessToken;
    }

    private final class Responder implements Runnable {
        private final Command mCommand;
        private final InstanceCredential mCredential;
        private final Runnable mCompletionHandler;
        private boolean mForwardedToIOThread = false;

        public Responder(
                Command command,
                InstanceCredential credential,
                Runnable completionHandler) {
            assert command != null;
            assert credential != null;
            assert completionHandler != null;

            mCommand = command;
            mCredential = credential;
            mCompletionHandler = completionHandler;
        }

        @Override
        public void run() {
            if (!mForwardedToIOThread) {
                mForwardedToIOThread = true;
                mIOExecutor.submit(this);
                return;
            }
            try {
                mClientFactory.newGCDClient(getAccessToken(mCredential)).patchCommand(mCommand);
            } catch (Exception e) {
                // TODO(serya): Handle authorization exception.
                Log.e(TAG, "Failure when patching command", e);
            }
        }
    }
}
