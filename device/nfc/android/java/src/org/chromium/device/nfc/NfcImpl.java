// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.nfc;

import android.Manifest;
import android.annotation.TargetApi;
import android.app.Activity;
import android.content.Context;
import android.content.pm.PackageManager;
import android.nfc.FormatException;
import android.nfc.NfcAdapter;
import android.nfc.NfcAdapter.ReaderCallback;
import android.nfc.NfcManager;
import android.nfc.Tag;
import android.nfc.TagLostException;
import android.os.Build;
import android.os.Process;

import org.chromium.base.Log;
import org.chromium.device.nfc.mojom.Nfc;
import org.chromium.device.nfc.mojom.NfcClient;
import org.chromium.device.nfc.mojom.NfcError;
import org.chromium.device.nfc.mojom.NfcErrorType;
import org.chromium.device.nfc.mojom.NfcMessage;
import org.chromium.device.nfc.mojom.NfcPushOptions;
import org.chromium.device.nfc.mojom.NfcPushTarget;
import org.chromium.device.nfc.mojom.NfcWatchOptions;
import org.chromium.mojo.bindings.Callbacks;
import org.chromium.mojo.system.MojoException;

import java.io.IOException;

/**
 * Android implementation of the NFC mojo service defined in
 * device/nfc/nfc.mojom.
 */
public class NfcImpl implements Nfc {
    private static final String TAG = "NfcImpl";

    /**
     * Used to get instance of NFC adapter, @see android.nfc.NfcManager
     */
    private final NfcManager mNfcManager;

    /**
     * NFC adapter. @see android.nfc.NfcAdapter
     */
    private final NfcAdapter mNfcAdapter;

    /**
     * Activity that is in foreground and is used to enable / disable NFC reader mode operations.
     * Can be updated when activity associated with web page is changed. @see #setActivity
     */
    private Activity mActivity;

    /**
     * Flag that indicates whether NFC permission is granted.
     */
    private final boolean mHasPermission;

    /**
     * Implementation of android.nfc.NfcAdapter.ReaderCallback. @see ReaderCallbackHandler
     */
    private ReaderCallbackHandler mReaderCallbackHandler;

    /**
     * Object that contains data that was passed to method
     * #push(NfcMessage message, NfcPushOptions options, PushResponse callback)
     * @see PendingPushOperation
     */
    private PendingPushOperation mPendingPushOperation;

    /**
     * Utility that provides I/O operations for a Tag. Created on demand when Tag is found.
     * @see NfcTagHandler
     */
    private NfcTagHandler mTagHandler;

    public NfcImpl(Context context) {
        int permission =
                context.checkPermission(Manifest.permission.NFC, Process.myPid(), Process.myUid());
        mHasPermission = permission == PackageManager.PERMISSION_GRANTED;

        if (!mHasPermission || Build.VERSION.SDK_INT < Build.VERSION_CODES.KITKAT) {
            Log.w(TAG, "NFC operations are not permitted.");
            mNfcAdapter = null;
            mNfcManager = null;
        } else {
            mNfcManager = (NfcManager) context.getSystemService(Context.NFC_SERVICE);
            if (mNfcManager == null) {
                Log.w(TAG, "NFC is not supported.");
                mNfcAdapter = null;
            } else {
                mNfcAdapter = mNfcManager.getDefaultAdapter();
            }
        }
    }

    /**
     * Sets Activity that is used to enable / disable NFC reader mode. When Activity is set,
     * reader mode is disabled for old Activity and enabled for the new Activity.
     */
    protected void setActivity(Activity activity) {
        disableReaderMode();
        mActivity = activity;
        enableReaderMode();
    }

    /**
     * Sets NfcClient. NfcClient interface is used to notify mojo NFC service client when NFC
     * device is in proximity and has NfcMessage that matches NfcWatchOptions criteria.
     * @see Nfc#watch(NfcWatchOptions options, WatchResponse callback)
     *
     * @param client @see NfcClient
     */
    @Override
    public void setClient(NfcClient client) {
        // TODO(crbug.com/625589): Should be implemented when watch() is implemented.
    }

    /**
     * Pushes NfcMessage to Tag or Peer, whenever NFC device is in proximity. At the moment, only
     * passive NFC devices are supported (NfcPushTarget.TAG).
     *
     * @param message that should be pushed to NFC device.
     * @param options that contain information about timeout and target device type.
     * @param callback that is used to notify when push operation is completed.
     */
    @Override
    public void push(NfcMessage message, NfcPushOptions options, PushResponse callback) {
        if (!checkIfReady(callback)) return;

        if (options.target == NfcPushTarget.PEER) {
            callback.call(createError(NfcErrorType.NOT_SUPPORTED));
            return;
        }

        // If previous pending push operation is not completed, cancel it.
        if (mPendingPushOperation != null) {
            mPendingPushOperation.complete(createError(NfcErrorType.OPERATION_CANCELLED));
        }

        mPendingPushOperation = new PendingPushOperation(message, options, callback);
        enableReaderMode();
        processPendingPushOperation();
    }

    /**
     * Cancels pending push operation.
     * At the moment, only passive NFC devices are supported (NfcPushTarget.TAG).
     *
     * @param target @see NfcPushTarget
     * @param callback that is used to notify caller when cancelPush() is completed.
     */
    @Override
    public void cancelPush(int target, CancelPushResponse callback) {
        if (!checkIfReady(callback)) return;

        if (target == NfcPushTarget.PEER) {
            callback.call(createError(NfcErrorType.NOT_SUPPORTED));
            return;
        }

        if (mPendingPushOperation == null) {
            callback.call(createError(NfcErrorType.NOT_FOUND));
        } else {
            mPendingPushOperation.complete(createError(NfcErrorType.OPERATION_CANCELLED));
            mPendingPushOperation = null;
            callback.call(null);
            disableReaderMode();
        }
    }

    /**
     * Watch method allows to set filtering criteria for NfcMessages that are found when NFC device
     * is within proximity. On success, watch ID is returned to caller through WatchResponse
     * callback. When NfcMessage that matches NfcWatchOptions is found, it is passed to NfcClient
     * interface together with corresponding watch ID.
     * @see NfcClient#onWatch(int[] id, NfcMessage message)
     *
     * @param options used to filter NfcMessages, @see NfcWatchOptions.
     * @param callback that is used to notify caller when watch() is completed and return watch ID.
     */
    @Override
    public void watch(NfcWatchOptions options, WatchResponse callback) {
        if (!checkIfReady(callback)) return;
        // TODO(crbug.com/625589): Not implemented.
        callback.call(0, createError(NfcErrorType.NOT_SUPPORTED));
    }

    /**
     * Cancels NFC watch operation.
     *
     * @param id of watch operation.
     * @param callback that is used to notify caller when cancelWatch() is completed.
     */
    @Override
    public void cancelWatch(int id, CancelWatchResponse callback) {
        if (!checkIfReady(callback)) return;
        // TODO(crbug.com/625589): Not implemented.
        callback.call(createError(NfcErrorType.NOT_SUPPORTED));
    }

    /**
     * Cancels all NFC watch operations.
     *
     * @param callback that is used to notify caller when cancelAllWatches() is completed.
     */
    @Override
    public void cancelAllWatches(CancelAllWatchesResponse callback) {
        if (!checkIfReady(callback)) return;
        // TODO(crbug.com/625589): Not implemented.
        callback.call(createError(NfcErrorType.NOT_SUPPORTED));
    }

    /**
     * Suspends all pending operations. Should be called when web page visibility is lost.
     */
    @Override
    public void suspendNfcOperations() {
        disableReaderMode();
    }

    /**
     * Resumes all pending watch / push operations. Should be called when web page becomes visible.
     */
    @Override
    public void resumeNfcOperations() {
        enableReaderMode();
    }

    @Override
    public void close() {
        disableReaderMode();
    }

    @Override
    public void onConnectionError(MojoException e) {
        close();
    }

    /**
     * Holds information about pending push operation.
     */
    private static class PendingPushOperation {
        public final NfcMessage nfcMessage;
        public final NfcPushOptions nfcPushOptions;
        private final PushResponse mPushResponseCallback;

        public PendingPushOperation(
                NfcMessage message, NfcPushOptions options, PushResponse callback) {
            nfcMessage = message;
            nfcPushOptions = options;
            mPushResponseCallback = callback;
        }

        /**
         * Completes pending push operation.
         *
         * @param error should be null when operation is completed successfully, otherwise,
         * error object with corresponding NfcErrorType should be provided.
         */
        public void complete(NfcError error) {
            if (mPushResponseCallback != null) mPushResponseCallback.call(error);
        }
    }

    /**
     * Helper method that creates NfcError object from NfcErrorType.
     */
    private NfcError createError(int errorType) {
        NfcError error = new NfcError();
        error.errorType = errorType;
        return error;
    }

    /**
     * Checks if NFC funcionality can be used by the mojo service. If permission to use NFC is
     * granted and hardware is enabled, returns null.
     */
    private NfcError checkIfReady() {
        if (!mHasPermission || mActivity == null) {
            return createError(NfcErrorType.SECURITY);
        } else if (mNfcManager == null || mNfcAdapter == null) {
            return createError(NfcErrorType.NOT_SUPPORTED);
        } else if (!mNfcAdapter.isEnabled()) {
            return createError(NfcErrorType.DEVICE_DISABLED);
        }
        return null;
    }

    /**
     * Uses checkIfReady() method and if NFC cannot be used, calls mojo callback with NfcError.
     *
     * @param WatchResponse Callback that is provided to watch() method.
     * @return boolean true if NFC functionality can be used, false otherwise.
     */
    private boolean checkIfReady(WatchResponse callback) {
        NfcError error = checkIfReady();
        if (error == null) return true;

        callback.call(0, error);
        return false;
    }

    /**
     * Uses checkIfReady() method and if NFC cannot be used, calls mojo callback with NfcError.
     *
     * @param callback Generic callback that is provided to push(), cancelPush(),
     * cancelWatch() and cancelAllWatches() methods.
     * @return boolean true if NFC functionality can be used, false otherwise.
     */
    private boolean checkIfReady(Callbacks.Callback1<NfcError> callback) {
        NfcError error = checkIfReady();
        if (error == null) return true;

        callback.call(error);
        return false;
    }

    /**
     * Implementation of android.nfc.NfcAdapter.ReaderCallback. Callback is called when NFC tag is
     * discovered, Tag object is delegated to mojo service implementation method
     * NfcImpl.onTagDiscovered().
     */
    @TargetApi(Build.VERSION_CODES.KITKAT)
    private static class ReaderCallbackHandler implements ReaderCallback {
        private final NfcImpl mNfcImpl;

        public ReaderCallbackHandler(NfcImpl impl) {
            mNfcImpl = impl;
        }

        @Override
        public void onTagDiscovered(Tag tag) {
            mNfcImpl.onTagDiscovered(tag);
        }
    }

    /**
     * Enables reader mode, allowing NFC device to read / write NFC tags.
     * @see android.nfc.NfcAdapter#enableReaderMode
     */
    private void enableReaderMode() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.KITKAT) return;

        if (mReaderCallbackHandler != null || mActivity == null || mNfcAdapter == null) return;

        // TODO(crbug.com/625589): Check if there are active watch operations.
        if (mPendingPushOperation == null) return;

        mReaderCallbackHandler = new ReaderCallbackHandler(this);
        mNfcAdapter.enableReaderMode(mActivity, mReaderCallbackHandler,
                NfcAdapter.FLAG_READER_NFC_A | NfcAdapter.FLAG_READER_NFC_B
                        | NfcAdapter.FLAG_READER_NFC_F | NfcAdapter.FLAG_READER_NFC_V,
                null);
    }

    /**
     * Disables reader mode.
     * @see android.nfc.NfcAdapter#disableReaderMode
     */
    @TargetApi(Build.VERSION_CODES.KITKAT)
    private void disableReaderMode() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.KITKAT) return;

        // There is no API that could query whether reader mode is enabled for adapter.
        // If mReaderCallbackHandler is null, reader mode is not enabled.
        if (mReaderCallbackHandler == null) return;

        mReaderCallbackHandler = null;
        if (mActivity != null && mNfcAdapter != null && !mActivity.isDestroyed()) {
            mNfcAdapter.disableReaderMode(mActivity);
        }
    }

    /**
     * Completes pending push operation. On error, invalidates #mTagHandler.
     */
    private void pendingPushOperationCompleted(NfcError error) {
        if (mPendingPushOperation != null) {
            mPendingPushOperation.complete(error);
            mPendingPushOperation = null;

            // TODO(crbug.com/625589): When nfc.watch is implemented, disable reader mode
            // only when there are no active watch operations.
            disableReaderMode();
        }

        if (error != null) mTagHandler = null;
    }

    /**
     * Checks whether there is a #mPendingPushOperation and writes data to NFC tag. In case of
     * exception calls pendingPushOperationCompleted() with appropriate error object.
     */
    private void processPendingPushOperation() {
        if (mTagHandler == null || mPendingPushOperation == null) return;

        if (mTagHandler.isTagOutOfRange()) {
            mTagHandler = null;
            return;
        }

        try {
            mTagHandler.connect();
            mTagHandler.write(NfcTypeConverter.toNdefMessage(mPendingPushOperation.nfcMessage));
            pendingPushOperationCompleted(null);
            mTagHandler.close();
        } catch (InvalidNfcMessageException e) {
            Log.w(TAG, "Cannot write data to NFC tag. Invalid NfcMessage.");
            pendingPushOperationCompleted(createError(NfcErrorType.INVALID_MESSAGE));
        } catch (TagLostException e) {
            Log.w(TAG, "Cannot write data to NFC tag. Tag is lost.");
            pendingPushOperationCompleted(createError(NfcErrorType.IO_ERROR));
        } catch (FormatException | IOException e) {
            Log.w(TAG, "Cannot write data to NFC tag. IO_ERROR.");
            pendingPushOperationCompleted(createError(NfcErrorType.IO_ERROR));
        }
    }

    /**
     * Called by ReaderCallbackHandler when NFC tag is in proximity.
     */
    public void onTagDiscovered(Tag tag) {
        mTagHandler = NfcTagHandler.create(tag);
        processPendingPushOperation();
    }
}
