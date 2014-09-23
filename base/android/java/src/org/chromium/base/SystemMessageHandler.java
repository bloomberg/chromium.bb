// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.os.MessageQueue;
import android.util.Log;

import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

class SystemMessageHandler extends Handler {

    private static final String TAG = "SystemMessageHandler";

    private static final int SCHEDULED_WORK = 1;
    private static final int DELAYED_SCHEDULED_WORK = 2;

    // Native class pointer set by the constructor of the SharedClient native class.
    private long mMessagePumpDelegateNative = 0;
    private long mDelayedScheduledTimeTicks = 0;

    // The following members are used to detect and trace the presence of sync
    // barriers in Android's MessageQueue. Note that this detection is
    // experimental, temporary and intended only for diagnostic purposes.
    private MessageQueue mMessageQueue;
    private Field mMessageQueueMessageField;
    private Field mMessageTargetField;
    private boolean mQueueHasSyncBarrier;
    private long mSyncBarrierTraceId;

    private SystemMessageHandler(long messagePumpDelegateNative) {
        mMessagePumpDelegateNative = messagePumpDelegateNative;
        tryEnableSyncBarrierDetection();
     }

    @Override
    public void handleMessage(Message msg) {
        updateWhetherQueueHasBlockingSyncBarrier();
        if (msg.what == DELAYED_SCHEDULED_WORK) {
            mDelayedScheduledTimeTicks = 0;
        }
        nativeDoRunLoopOnce(mMessagePumpDelegateNative, mDelayedScheduledTimeTicks);
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void scheduleWork() {
        updateWhetherQueueHasBlockingSyncBarrier();
        if (mQueueHasSyncBarrier) TraceEvent.instant("SystemMessageHandler:immediateWorkBlocked");
        sendEmptyMessage(SCHEDULED_WORK);
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void scheduleDelayedWork(long delayedTimeTicks, long millis) {
        if (mDelayedScheduledTimeTicks != 0) {
            removeMessages(DELAYED_SCHEDULED_WORK);
        }
        mDelayedScheduledTimeTicks = delayedTimeTicks;
        updateWhetherQueueHasBlockingSyncBarrier();
        if (mQueueHasSyncBarrier) TraceEvent.instant("SystemMessageHandler:delayedWorkBlocked");
        sendEmptyMessageDelayed(DELAYED_SCHEDULED_WORK, millis);
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void removeAllPendingMessages() {
        updateWhetherQueueHasBlockingSyncBarrier();
        removeMessages(SCHEDULED_WORK);
        removeMessages(DELAYED_SCHEDULED_WORK);
    }

    private void updateWhetherQueueHasBlockingSyncBarrier() {
        if (mMessageQueue == null) return;
        // As barrier detection is only used for tracing, early out when tracing
        // is disabled to avoid any potential performance penalties.
        if (!TraceEvent.enabled()) {
            mQueueHasSyncBarrier = false;
            return;
        }
        Message queueHead = (Message)getField(mMessageQueue, mMessageQueueMessageField);
        setqueueHasSyncBarrier(isSyncBarrierMessage(queueHead));
    }

    private boolean isSyncBarrierMessage(Message message) {
        if (message == null) return false;
        // Sync barrier messages have null targets.
        return getField(message, mMessageTargetField) == null;
    }

    private void tryEnableSyncBarrierDetection() {
        assert mMessageQueue == null;

        boolean success = false;
        try {
            Method getQueueMethod = Looper.class.getMethod("getQueue", new Class[]{});
            mMessageQueue = (MessageQueue)getQueueMethod.invoke(getLooper());

            mMessageQueueMessageField = mMessageQueue.getClass().getDeclaredField("mMessages");
            mMessageQueueMessageField.setAccessible(true);

            mMessageTargetField = Message.class.getDeclaredField("target");
            mMessageTargetField.setAccessible(true);

            mSyncBarrierTraceId = hashCode();

            success = true;
        } catch (NoSuchMethodException e) {
            Log.e(TAG, "Failed to load method: " + e);
        } catch (NoSuchFieldException e) {
            Log.e(TAG, "Failed to load field: " + e);
        } catch (InvocationTargetException e) {
            Log.e(TAG, "Failed invocation: " + e);
        } catch (IllegalAccessException e) {
            Log.e(TAG, "Illegal access to reflected invocation: " + e);
        } catch (IllegalArgumentException e) {
            Log.e(TAG, "Illegal argument to reflected invocation: " + e);
        } catch (RuntimeException e) {
            Log.e(TAG, e.toString());
        } finally {
            if (!success) disableSyncBarrierDetection();
        }
    }

    private void disableSyncBarrierDetection() {
        Log.e(TAG, "Unexpected error with sync barrier detection, disabling.");
        mMessageQueue = null;
        mMessageQueueMessageField = null;
        mMessageTargetField = null;
        setqueueHasSyncBarrier(false);
    }

    private void setqueueHasSyncBarrier(boolean queueHasSyncBarrier) {
        if (queueHasSyncBarrier == mQueueHasSyncBarrier) return;
        mQueueHasSyncBarrier = queueHasSyncBarrier;
        if (mQueueHasSyncBarrier) {
            TraceEvent.startAsync("SyncBarrier", mSyncBarrierTraceId);
        } else {
            TraceEvent.finishAsync("SyncBarrier", mSyncBarrierTraceId);
        }
    }

    private Object getField(Object object, Field field) {
        try {
            return field.get(object);
        } catch (IllegalAccessException e) {
            Log.e(TAG, "Failed field access: " + e);
            disableSyncBarrierDetection();
        }
        return null;
     }

    @CalledByNative
    private static SystemMessageHandler create(long messagePumpDelegateNative) {
        return new SystemMessageHandler(messagePumpDelegateNative);
    }

    private native void nativeDoRunLoopOnce(
            long messagePumpDelegateNative, long delayedScheduledTimeTicks);
}
