// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.nfc;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.Context;
import android.content.pm.PackageManager;
import android.nfc.FormatException;
import android.nfc.NdefMessage;
import android.nfc.NdefRecord;
import android.nfc.NfcAdapter;
import android.nfc.NfcAdapter.ReaderCallback;
import android.nfc.NfcManager;
import android.os.Bundle;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.util.Feature;
import org.chromium.device.nfc.mojom.Nfc.CancelAllWatchesResponse;
import org.chromium.device.nfc.mojom.Nfc.CancelPushResponse;
import org.chromium.device.nfc.mojom.Nfc.CancelWatchResponse;
import org.chromium.device.nfc.mojom.Nfc.PushResponse;
import org.chromium.device.nfc.mojom.Nfc.WatchResponse;
import org.chromium.device.nfc.mojom.NfcClient;
import org.chromium.device.nfc.mojom.NfcError;
import org.chromium.device.nfc.mojom.NfcErrorType;
import org.chromium.device.nfc.mojom.NfcMessage;
import org.chromium.device.nfc.mojom.NfcPushOptions;
import org.chromium.device.nfc.mojom.NfcPushTarget;
import org.chromium.device.nfc.mojom.NfcRecord;
import org.chromium.device.nfc.mojom.NfcRecordType;
import org.chromium.device.nfc.mojom.NfcRecordTypeFilter;
import org.chromium.device.nfc.mojom.NfcWatchMode;
import org.chromium.device.nfc.mojom.NfcWatchOptions;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.io.IOException;
import java.io.UnsupportedEncodingException;

/**
 * Unit tests for NfcImpl and NfcTypeConverter classes.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NFCTest {
    @Mock
    private Context mContext;
    @Mock
    private NfcManager mNfcManager;
    @Mock
    private NfcAdapter mNfcAdapter;
    @Mock
    private Activity mActivity;
    @Mock
    private NfcClient mNfcClient;
    @Mock
    private NfcTagHandler mNfcTagHandler;
    @Captor
    private ArgumentCaptor<NfcError> mErrorCaptor;
    @Captor
    private ArgumentCaptor<Integer> mWatchCaptor;
    @Captor
    private ArgumentCaptor<int[]> mOnWatchCallbackCaptor;

    // Constants used for the test.
    private static final String TEST_TEXT = "test";
    private static final String TEST_URL = "https://google.com";
    private static final String TEST_JSON = "{\"key1\":\"value1\",\"key2\":2}";
    private static final String DOMAIN = "w3.org";
    private static final String TYPE = "webnfc";
    private static final String TEXT_MIME = "text/plain";
    private static final String JSON_MIME = "application/json";
    private static final String CHARSET_UTF8 = ";charset=UTF-8";
    private static final String CHARSET_UTF16 = ";charset=UTF-16";
    private static final String LANG_EN_US = "en-US";

    /**
     * Class that is used test NfcImpl implementation
     */
    private static class TestNfcImpl extends NfcImpl {
        public TestNfcImpl(Context context) {
            super(context);
        }

        public void setActivityForTesting(Activity activity) {
            super.setActivity(activity);
        }

        public void processPendingOperationsForTesting(NfcTagHandler handler) {
            super.processPendingOperations(handler);
        }
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        doReturn(mNfcManager).when(mContext).getSystemService(Context.NFC_SERVICE);
        doReturn(mNfcAdapter).when(mNfcManager).getDefaultAdapter();
        doReturn(true).when(mNfcAdapter).isEnabled();
        doReturn(PackageManager.PERMISSION_GRANTED)
                .when(mContext)
                .checkPermission(anyString(), anyInt(), anyInt());
        doNothing()
                .when(mNfcAdapter)
                .enableReaderMode(any(Activity.class), any(ReaderCallback.class), anyInt(),
                        (Bundle) isNull());
        doNothing().when(mNfcAdapter).disableReaderMode(any(Activity.class));
        // Tag handler overrides used to mock connected tag.
        doReturn(true).when(mNfcTagHandler).isConnected();
        doReturn(false).when(mNfcTagHandler).isTagOutOfRange();
        try {
            doNothing().when(mNfcTagHandler).connect();
            doNothing().when(mNfcTagHandler).write(any(NdefMessage.class));
            doReturn(createUrlWebNFCNdefMessage()).when(mNfcTagHandler).read();
            doNothing().when(mNfcTagHandler).close();
        } catch (IOException | FormatException e) {
        }
    }

    /**
     * Test that error with type NOT_SUPPORTED is returned if NFC is not supported.
     */
    @Test
    @Feature({"NFCTest"})
    public void testNFCNotSupported() {
        doReturn(null).when(mNfcManager).getDefaultAdapter();
        TestNfcImpl nfc = new TestNfcImpl(mContext);
        nfc.setActivityForTesting(mActivity);
        CancelAllWatchesResponse mockCallback = mock(CancelAllWatchesResponse.class);
        nfc.cancelAllWatches(mockCallback);
        verify(mockCallback).call(mErrorCaptor.capture());
        assertEquals(NfcErrorType.NOT_SUPPORTED, mErrorCaptor.getValue().errorType);
    }

    /**
     * Test that error with type SECURITY is returned if permission to use NFC is not granted.
     */
    @Test
    @Feature({"NFCTest"})
    public void testNFCIsNotPermitted() {
        doReturn(PackageManager.PERMISSION_DENIED)
                .when(mContext)
                .checkPermission(anyString(), anyInt(), anyInt());
        TestNfcImpl nfc = new TestNfcImpl(mContext);
        CancelAllWatchesResponse mockCallback = mock(CancelAllWatchesResponse.class);
        nfc.cancelAllWatches(mockCallback);
        verify(mockCallback).call(mErrorCaptor.capture());
        assertEquals(NfcErrorType.SECURITY, mErrorCaptor.getValue().errorType);
    }

    /**
     * Test that method can be invoked successfully if NFC is supported and adapter is enabled.
     */
    @Test
    @Feature({"NFCTest"})
    public void testNFCIsSupported() {
        TestNfcImpl nfc = new TestNfcImpl(mContext);
        nfc.setActivityForTesting(mActivity);
        WatchResponse mockCallback = mock(WatchResponse.class);
        nfc.watch(createNfcWatchOptions(), mockCallback);
        verify(mockCallback).call(anyInt(), mErrorCaptor.capture());
        assertNull(mErrorCaptor.getValue());
    }

    /**
     * Test conversion from NdefMessage to mojo NfcMessage.
     */
    @Test
    @Feature({"NFCTest"})
    public void testNdefToMojoConversion() throws UnsupportedEncodingException {
        // Test EMPTY record conversion.
        NdefMessage emptyNdefMessage =
                new NdefMessage(new NdefRecord(NdefRecord.TNF_EMPTY, null, null, null));
        NfcMessage emptyNfcMessage = NfcTypeConverter.toNfcMessage(emptyNdefMessage);
        assertNull(emptyNfcMessage.url);
        assertEquals(1, emptyNfcMessage.data.length);
        assertEquals(NfcRecordType.EMPTY, emptyNfcMessage.data[0].recordType);
        assertEquals(true, emptyNfcMessage.data[0].mediaType.isEmpty());
        assertEquals(0, emptyNfcMessage.data[0].data.length);

        // Test URL record conversion.
        NdefMessage urlNdefMessage = new NdefMessage(NdefRecord.createUri(TEST_URL));
        NfcMessage urlNfcMessage = NfcTypeConverter.toNfcMessage(urlNdefMessage);
        assertNull(urlNfcMessage.url);
        assertEquals(1, urlNfcMessage.data.length);
        assertEquals(NfcRecordType.URL, urlNfcMessage.data[0].recordType);
        assertEquals(TEXT_MIME, urlNfcMessage.data[0].mediaType);
        assertEquals(TEST_URL, new String(urlNfcMessage.data[0].data));

        // Test TEXT record conversion.
        NdefMessage textNdefMessage =
                new NdefMessage(NdefRecord.createTextRecord(LANG_EN_US, TEST_TEXT));
        NfcMessage textNfcMessage = NfcTypeConverter.toNfcMessage(textNdefMessage);
        assertNull(textNfcMessage.url);
        assertEquals(1, textNfcMessage.data.length);
        assertEquals(NfcRecordType.TEXT, textNfcMessage.data[0].recordType);
        assertEquals(TEXT_MIME, textNfcMessage.data[0].mediaType);
        assertEquals(TEST_TEXT, new String(textNfcMessage.data[0].data));

        // Test MIME record conversion.
        NdefMessage mimeNdefMessage =
                new NdefMessage(NdefRecord.createMime(TEXT_MIME, TEST_TEXT.getBytes()));
        NfcMessage mimeNfcMessage = NfcTypeConverter.toNfcMessage(mimeNdefMessage);
        assertNull(mimeNfcMessage.url);
        assertEquals(1, mimeNfcMessage.data.length);
        assertEquals(NfcRecordType.OPAQUE_RECORD, mimeNfcMessage.data[0].recordType);
        assertEquals(TEXT_MIME, textNfcMessage.data[0].mediaType);
        assertEquals(TEST_TEXT, new String(textNfcMessage.data[0].data));

        // Test JSON record conversion.
        NdefMessage jsonNdefMessage =
                new NdefMessage(NdefRecord.createMime(JSON_MIME, TEST_JSON.getBytes()));
        NfcMessage jsonNfcMessage = NfcTypeConverter.toNfcMessage(jsonNdefMessage);
        assertNull(jsonNfcMessage.url);
        assertEquals(1, jsonNfcMessage.data.length);
        assertEquals(NfcRecordType.JSON, jsonNfcMessage.data[0].recordType);
        assertEquals(JSON_MIME, jsonNfcMessage.data[0].mediaType);
        assertEquals(TEST_JSON, new String(jsonNfcMessage.data[0].data));

        // Test NfcMessage with WebNFC external type.
        NdefRecord jsonNdefRecord = NdefRecord.createMime(JSON_MIME, TEST_JSON.getBytes());
        NdefRecord extNdefRecord = NdefRecord.createExternal(DOMAIN, TYPE, TEST_URL.getBytes());
        NdefMessage webNdefMessage = new NdefMessage(jsonNdefRecord, extNdefRecord);
        NfcMessage webNfcMessage = NfcTypeConverter.toNfcMessage(webNdefMessage);
        assertEquals(TEST_URL, webNfcMessage.url);
        assertEquals(1, webNfcMessage.data.length);
        assertEquals(NfcRecordType.JSON, webNfcMessage.data[0].recordType);
        assertEquals(JSON_MIME, webNfcMessage.data[0].mediaType);
        assertEquals(TEST_JSON, new String(webNfcMessage.data[0].data));
    }

    /**
     * Test conversion from mojo NfcMessage to android NdefMessage.
     */
    @Test
    @Feature({"NFCTest"})
    public void testMojoToNdefConversion() throws InvalidNfcMessageException {
        // Test URL record conversion.
        NdefMessage urlNdefMessage = createUrlWebNFCNdefMessage();
        assertEquals(2, urlNdefMessage.getRecords().length);
        assertEquals(NdefRecord.TNF_WELL_KNOWN, urlNdefMessage.getRecords()[0].getTnf());
        assertEquals(TEST_URL, urlNdefMessage.getRecords()[0].toUri().toString());
        assertEquals(NdefRecord.TNF_EXTERNAL_TYPE, urlNdefMessage.getRecords()[1].getTnf());
        assertEquals(DOMAIN + ":" + TYPE, new String(urlNdefMessage.getRecords()[1].getType()));

        // Test TEXT record conversion.
        NfcRecord textNfcRecord = new NfcRecord();
        textNfcRecord.recordType = NfcRecordType.TEXT;
        textNfcRecord.mediaType = TEXT_MIME;
        textNfcRecord.data = TEST_TEXT.getBytes();
        NfcMessage textNfcMessage = createNfcMessage(TEST_URL, textNfcRecord);
        NdefMessage textNdefMessage = NfcTypeConverter.toNdefMessage(textNfcMessage);
        assertEquals(2, textNdefMessage.getRecords().length);
        short tnf = textNdefMessage.getRecords()[0].getTnf();
        boolean isWellKnownOrMime =
                (tnf == NdefRecord.TNF_WELL_KNOWN || tnf == NdefRecord.TNF_MIME_MEDIA);
        assertEquals(true, isWellKnownOrMime);
        assertEquals(NdefRecord.TNF_EXTERNAL_TYPE, textNdefMessage.getRecords()[1].getTnf());

        // Test MIME record conversion.
        NfcRecord mimeNfcRecord = new NfcRecord();
        mimeNfcRecord.recordType = NfcRecordType.OPAQUE_RECORD;
        mimeNfcRecord.mediaType = TEXT_MIME;
        mimeNfcRecord.data = TEST_TEXT.getBytes();
        NfcMessage mimeNfcMessage = createNfcMessage(TEST_URL, mimeNfcRecord);
        NdefMessage mimeNdefMessage = NfcTypeConverter.toNdefMessage(mimeNfcMessage);
        assertEquals(2, mimeNdefMessage.getRecords().length);
        assertEquals(NdefRecord.TNF_MIME_MEDIA, mimeNdefMessage.getRecords()[0].getTnf());
        assertEquals(TEXT_MIME, mimeNdefMessage.getRecords()[0].toMimeType());
        assertEquals(TEST_TEXT, new String(mimeNdefMessage.getRecords()[0].getPayload()));
        assertEquals(NdefRecord.TNF_EXTERNAL_TYPE, mimeNdefMessage.getRecords()[1].getTnf());

        // Test JSON record conversion.
        NfcRecord jsonNfcRecord = new NfcRecord();
        jsonNfcRecord.recordType = NfcRecordType.OPAQUE_RECORD;
        jsonNfcRecord.mediaType = JSON_MIME;
        jsonNfcRecord.data = TEST_JSON.getBytes();
        NfcMessage jsonNfcMessage = createNfcMessage(TEST_URL, jsonNfcRecord);
        NdefMessage jsonNdefMessage = NfcTypeConverter.toNdefMessage(jsonNfcMessage);
        assertEquals(2, jsonNdefMessage.getRecords().length);
        assertEquals(NdefRecord.TNF_MIME_MEDIA, jsonNdefMessage.getRecords()[0].getTnf());
        assertEquals(JSON_MIME, jsonNdefMessage.getRecords()[0].toMimeType());
        assertEquals(TEST_JSON, new String(jsonNdefMessage.getRecords()[0].getPayload()));
        assertEquals(NdefRecord.TNF_EXTERNAL_TYPE, jsonNdefMessage.getRecords()[1].getTnf());
    }

    /**
     * Test that invalid NfcMessage is rejected with INVALID_MESSAGE error code.
     */
    @Test
    @Feature({"NFCTest"})
    public void testInvalidNfcMessage() {
        TestNfcImpl nfc = new TestNfcImpl(mContext);
        nfc.setActivityForTesting(mActivity);
        PushResponse mockCallback = mock(PushResponse.class);
        nfc.push(new NfcMessage(), createNfcPushOptions(), mockCallback);
        nfc.processPendingOperationsForTesting(mNfcTagHandler);
        verify(mockCallback).call(mErrorCaptor.capture());
        assertEquals(NfcErrorType.INVALID_MESSAGE, mErrorCaptor.getValue().errorType);
    }

    /**
     * Test that Nfc.suspendNfcOperations() and Nfc.resumeNfcOperations() work correctly.
     */
    @Test
    @Feature({"NFCTest"})
    public void testResumeSuspend() {
        TestNfcImpl nfc = new TestNfcImpl(mContext);
        // No activity / client or active pending operations
        nfc.suspendNfcOperations();
        nfc.resumeNfcOperations();

        nfc.setActivityForTesting(mActivity);
        nfc.setClient(mNfcClient);
        WatchResponse mockCallback = mock(WatchResponse.class);
        nfc.watch(createNfcWatchOptions(), mockCallback);
        nfc.suspendNfcOperations();
        verify(mNfcAdapter, times(1)).disableReaderMode(mActivity);
        nfc.resumeNfcOperations();
        // 1. Enable after watch is called, 2. after resumeNfcOperations is called.
        verify(mNfcAdapter, times(2))
                .enableReaderMode(any(Activity.class), any(ReaderCallback.class), anyInt(),
                        (Bundle) isNull());

        nfc.processPendingOperationsForTesting(mNfcTagHandler);
        // Check that watch request was completed successfully.
        verify(mockCallback).call(mWatchCaptor.capture(), mErrorCaptor.capture());
        assertNull(mErrorCaptor.getValue());

        // Check that client was notified and watch with correct id was triggered.
        verify(mNfcClient, times(1))
                .onWatch(mOnWatchCallbackCaptor.capture(), any(NfcMessage.class));
        assertEquals(mWatchCaptor.getValue().intValue(), mOnWatchCallbackCaptor.getValue()[0]);
    }

    /**
     * Test that Nfc.push() successful when NFC tag is connected.
     */
    @Test
    @Feature({"NFCTest"})
    public void testPush() {
        TestNfcImpl nfc = new TestNfcImpl(mContext);
        nfc.setActivityForTesting(mActivity);
        PushResponse mockCallback = mock(PushResponse.class);
        nfc.push(createNfcMessage(), createNfcPushOptions(), mockCallback);
        nfc.processPendingOperationsForTesting(mNfcTagHandler);
        verify(mockCallback).call(mErrorCaptor.capture());
        assertNull(mErrorCaptor.getValue());
    }

    /**
     * Test that Nfc.cancelPush() cancels pending push opration and completes successfully.
     */
    @Test
    @Feature({"NFCTest"})
    public void testCancelPush() {
        TestNfcImpl nfc = new TestNfcImpl(mContext);
        nfc.setActivityForTesting(mActivity);
        PushResponse mockPushCallback = mock(PushResponse.class);
        CancelPushResponse mockCancelPushCallback = mock(CancelPushResponse.class);
        nfc.push(createNfcMessage(), createNfcPushOptions(), mockPushCallback);
        nfc.cancelPush(NfcPushTarget.ANY, mockCancelPushCallback);

        // Check that push request was cancelled with OPERATION_CANCELLED.
        verify(mockPushCallback).call(mErrorCaptor.capture());
        assertEquals(NfcErrorType.OPERATION_CANCELLED, mErrorCaptor.getValue().errorType);

        // Check that cancel request was successfuly completed.
        verify(mockCancelPushCallback).call(mErrorCaptor.capture());
        assertNull(mErrorCaptor.getValue());
    }

    /**
     * Test that Nfc.watch() works correctly and client is notified.
     */
    @Test
    @Feature({"NFCTest"})
    public void testWatch() {
        TestNfcImpl nfc = new TestNfcImpl(mContext);
        nfc.setActivityForTesting(mActivity);
        nfc.setClient(mNfcClient);
        WatchResponse mockWatchCallback1 = mock(WatchResponse.class);
        nfc.watch(createNfcWatchOptions(), mockWatchCallback1);

        // Check that watch requests were completed successfully.
        verify(mockWatchCallback1).call(mWatchCaptor.capture(), mErrorCaptor.capture());
        assertNull(mErrorCaptor.getValue());
        int watchId1 = mWatchCaptor.getValue().intValue();

        WatchResponse mockWatchCallback2 = mock(WatchResponse.class);
        nfc.watch(createNfcWatchOptions(), mockWatchCallback2);
        verify(mockWatchCallback2).call(mWatchCaptor.capture(), mErrorCaptor.capture());
        assertNull(mErrorCaptor.getValue());
        int watchId2 = mWatchCaptor.getValue().intValue();
        // Check that each watch operation is associated with unique id.
        assertNotEquals(watchId1, watchId2);

        // Mocks 'NFC tag found' event.
        nfc.processPendingOperationsForTesting(mNfcTagHandler);

        // Check that client was notified and correct watch ids were provided.
        verify(mNfcClient, times(1))
                .onWatch(mOnWatchCallbackCaptor.capture(), any(NfcMessage.class));
        assertEquals(watchId1, mOnWatchCallbackCaptor.getValue()[0]);
        assertEquals(watchId2, mOnWatchCallbackCaptor.getValue()[1]);
    }

    /**
     * Test that Nfc.watch() matching function works correctly.
     */
    @Test
    @Feature({"NFCTest"})
    public void testlWatchMatching() {
        TestNfcImpl nfc = new TestNfcImpl(mContext);
        nfc.setActivityForTesting(mActivity);
        nfc.setClient(mNfcClient);

        // Should match by WebNFC Id.
        NfcWatchOptions options1 = createNfcWatchOptions();
        options1.mode = NfcWatchMode.WEBNFC_ONLY;
        options1.url = TEST_URL;
        WatchResponse mockWatchCallback1 = mock(WatchResponse.class);
        nfc.watch(options1, mockWatchCallback1);
        verify(mockWatchCallback1).call(mWatchCaptor.capture(), mErrorCaptor.capture());
        assertNull(mErrorCaptor.getValue());
        int watchId1 = mWatchCaptor.getValue().intValue();

        // Should match by media type.
        NfcWatchOptions options2 = createNfcWatchOptions();
        options2.mode = NfcWatchMode.ANY;
        options2.mediaType = TEXT_MIME;
        WatchResponse mockWatchCallback2 = mock(WatchResponse.class);
        nfc.watch(options2, mockWatchCallback2);
        verify(mockWatchCallback2).call(mWatchCaptor.capture(), mErrorCaptor.capture());
        assertNull(mErrorCaptor.getValue());
        int watchId2 = mWatchCaptor.getValue().intValue();

        // Should match by record type.
        NfcWatchOptions options3 = createNfcWatchOptions();
        options3.mode = NfcWatchMode.ANY;
        NfcRecordTypeFilter typeFilter = new NfcRecordTypeFilter();
        typeFilter.recordType = NfcRecordType.URL;
        options3.recordFilter = typeFilter;
        WatchResponse mockWatchCallback3 = mock(WatchResponse.class);
        nfc.watch(options3, mockWatchCallback3);
        verify(mockWatchCallback3).call(mWatchCaptor.capture(), mErrorCaptor.capture());
        assertNull(mErrorCaptor.getValue());
        int watchId3 = mWatchCaptor.getValue().intValue();

        // Should not match
        NfcWatchOptions options4 = createNfcWatchOptions();
        options4.mode = NfcWatchMode.WEBNFC_ONLY;
        options4.url = DOMAIN;
        WatchResponse mockWatchCallback4 = mock(WatchResponse.class);
        nfc.watch(options4, mockWatchCallback4);
        verify(mockWatchCallback4).call(mWatchCaptor.capture(), mErrorCaptor.capture());
        assertNull(mErrorCaptor.getValue());
        int watchId4 = mWatchCaptor.getValue().intValue();

        nfc.processPendingOperationsForTesting(mNfcTagHandler);

        // Check that client was notified and watch with correct id was triggered.
        verify(mNfcClient, times(1))
                .onWatch(mOnWatchCallbackCaptor.capture(), any(NfcMessage.class));
        assertEquals(3, mOnWatchCallbackCaptor.getValue().length);
        assertEquals(watchId1, mOnWatchCallbackCaptor.getValue()[0]);
        assertEquals(watchId2, mOnWatchCallbackCaptor.getValue()[1]);
        assertEquals(watchId3, mOnWatchCallbackCaptor.getValue()[2]);
    }

    /**
     * Test that Nfc.watch() can be cancelled with Nfc.cancelWatch().
     */
    @Test
    @Feature({"NFCTest"})
    public void testCancelWatch() {
        TestNfcImpl nfc = new TestNfcImpl(mContext);
        nfc.setActivityForTesting(mActivity);
        WatchResponse mockWatchCallback = mock(WatchResponse.class);
        nfc.watch(createNfcWatchOptions(), mockWatchCallback);

        verify(mockWatchCallback).call(mWatchCaptor.capture(), mErrorCaptor.capture());
        assertNull(mErrorCaptor.getValue());

        CancelWatchResponse mockCancelWatchCallback = mock(CancelWatchResponse.class);
        nfc.cancelWatch(mWatchCaptor.getValue().intValue(), mockCancelWatchCallback);

        // Check that cancel request was successfuly completed.
        verify(mockCancelWatchCallback).call(mErrorCaptor.capture());
        assertNull(mErrorCaptor.getValue());

        // Check that watch is not triggered when NFC tag is in proximity.
        nfc.processPendingOperationsForTesting(mNfcTagHandler);
        verify(mNfcClient, times(0)).onWatch(any(int[].class), any(NfcMessage.class));
    }

    /**
     * Test that Nfc.cancelAllWatches() cancels all pending watch operations.
     */
    @Test
    @Feature({"NFCTest"})
    public void testCancelAllWatches() {
        TestNfcImpl nfc = new TestNfcImpl(mContext);
        nfc.setActivityForTesting(mActivity);
        WatchResponse mockWatchCallback1 = mock(WatchResponse.class);
        WatchResponse mockWatchCallback2 = mock(WatchResponse.class);
        nfc.watch(createNfcWatchOptions(), mockWatchCallback1);
        verify(mockWatchCallback1).call(mWatchCaptor.capture(), mErrorCaptor.capture());
        assertNull(mErrorCaptor.getValue());

        nfc.watch(createNfcWatchOptions(), mockWatchCallback2);
        verify(mockWatchCallback2).call(mWatchCaptor.capture(), mErrorCaptor.capture());
        assertNull(mErrorCaptor.getValue());

        CancelAllWatchesResponse mockCallback = mock(CancelAllWatchesResponse.class);
        nfc.cancelAllWatches(mockCallback);

        // Check that cancel request was successfuly completed.
        verify(mockCallback).call(mErrorCaptor.capture());
        assertNull(mErrorCaptor.getValue());
    }

    /**
     * Test that Nfc.cancelWatch() with invalid id is failing with NOT_FOUND error.
     */
    @Test
    @Feature({"NFCTest"})
    public void testCancelWatchInvalidId() {
        TestNfcImpl nfc = new TestNfcImpl(mContext);
        nfc.setActivityForTesting(mActivity);
        WatchResponse mockWatchCallback = mock(WatchResponse.class);
        nfc.watch(createNfcWatchOptions(), mockWatchCallback);

        verify(mockWatchCallback).call(mWatchCaptor.capture(), mErrorCaptor.capture());
        assertNull(mErrorCaptor.getValue());

        CancelWatchResponse mockCancelWatchCallback = mock(CancelWatchResponse.class);
        nfc.cancelWatch(mWatchCaptor.getValue().intValue() + 1, mockCancelWatchCallback);

        verify(mockCancelWatchCallback).call(mErrorCaptor.capture());
        assertEquals(NfcErrorType.NOT_FOUND, mErrorCaptor.getValue().errorType);
    }

    /**
     * Test that Nfc.cancelAllWatches() is failing with NOT_FOUND error if there are no active
     * watch opeartions.
     */
    @Test
    @Feature({"NFCTest"})
    public void testCancelAllWatchesWithNoWathcers() {
        TestNfcImpl nfc = new TestNfcImpl(mContext);
        nfc.setActivityForTesting(mActivity);
        CancelAllWatchesResponse mockCallback = mock(CancelAllWatchesResponse.class);
        nfc.cancelAllWatches(mockCallback);

        verify(mockCallback).call(mErrorCaptor.capture());
        assertEquals(NfcErrorType.NOT_FOUND, mErrorCaptor.getValue().errorType);
    }

    private NfcPushOptions createNfcPushOptions() {
        NfcPushOptions pushOptions = new NfcPushOptions();
        pushOptions.target = NfcPushTarget.ANY;
        pushOptions.timeout = 0;
        pushOptions.ignoreRead = false;
        return pushOptions;
    }

    private NfcWatchOptions createNfcWatchOptions() {
        NfcWatchOptions options = new NfcWatchOptions();
        options.url = "";
        options.mediaType = "";
        options.mode = NfcWatchMode.ANY;
        options.recordFilter = null;
        return options;
    }

    private NfcMessage createNfcMessage() {
        NfcMessage message = new NfcMessage();
        message.url = "";
        message.data = new NfcRecord[1];

        NfcRecord nfcRecord = new NfcRecord();
        nfcRecord.recordType = NfcRecordType.TEXT;
        nfcRecord.mediaType = TEXT_MIME;
        nfcRecord.data = TEST_TEXT.getBytes();
        message.data[0] = nfcRecord;
        return message;
    }

    private NfcMessage createNfcMessage(String url, NfcRecord record) {
        NfcMessage message = new NfcMessage();
        message.url = url;
        message.data = new NfcRecord[1];
        message.data[0] = record;
        return message;
    }

    private NdefMessage createUrlWebNFCNdefMessage() {
        NfcRecord urlNfcRecord = new NfcRecord();
        urlNfcRecord.recordType = NfcRecordType.URL;
        urlNfcRecord.mediaType = TEXT_MIME;
        urlNfcRecord.data = TEST_URL.getBytes();
        NfcMessage urlNfcMessage = createNfcMessage(TEST_URL, urlNfcRecord);
        try {
            return NfcTypeConverter.toNdefMessage(urlNfcMessage);
        } catch (InvalidNfcMessageException e) {
            return null;
        }
    }
}
