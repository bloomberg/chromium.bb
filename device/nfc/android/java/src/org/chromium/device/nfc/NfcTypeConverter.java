// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.nfc;

import android.nfc.NdefMessage;
import android.nfc.NdefRecord;
import android.os.Build;

import org.chromium.base.Log;
import org.chromium.mojom.device.nfc.NfcMessage;
import org.chromium.mojom.device.nfc.NfcRecord;
import org.chromium.mojom.device.nfc.NfcRecordType;

import java.io.UnsupportedEncodingException;
import java.util.ArrayList;
import java.util.List;

/**
 * Utility class that provides convesion between Android NdefMessage
 * and mojo NfcMessage data structures.
 */
public final class NfcTypeConverter {
    private static final String TAG = "NfcTypeConverter";
    private static final String DOMAIN = "w3.org";
    private static final String TYPE = "webnfc";
    private static final String WEBNFC_URN = DOMAIN + ":" + TYPE;
    private static final String TEXT_MIME = "text/plain";
    private static final String JSON_MIME = "application/json";
    private static final String CHARSET_UTF8 = ";charset=UTF-8";
    private static final String CHARSET_UTF16 = ";charset=UTF-16";

    /**
     * Converts mojo NfcMessage to android.nfc.NdefMessage
     */
    public static NdefMessage toNdefMessage(NfcMessage message) throws InvalidNfcMessageException {
        if (message == null || message.data.length == 0) throw new InvalidNfcMessageException();

        try {
            List<NdefRecord> records = new ArrayList<NdefRecord>();
            for (int i = 0; i < message.data.length; ++i) {
                records.add(toNdefRecord(message.data[i]));
            }
            records.add(NdefRecord.createExternal(DOMAIN, TYPE, message.url.getBytes("UTF-8")));
            NdefRecord[] ndefRecords = new NdefRecord[records.size()];
            records.toArray(ndefRecords);
            return new NdefMessage(ndefRecords);
        } catch (UnsupportedEncodingException | InvalidNfcMessageException
                | IllegalArgumentException e) {
            throw new InvalidNfcMessageException();
        }
    }

    /**
     * Returns charset of mojo NfcRecord. Only applicable for URL and TEXT records.
     * If charset cannot be determined, UTF-8 charset is used by default.
     */
    private static String getCharset(NfcRecord record) {
        if (record.mediaType.endsWith(CHARSET_UTF8)) return "UTF-8";

        // When 16bit WTF::String data is converted to bytearray, it is in LE byte order, without
        // BOM. By default, Android interprets UTF-16 charset without BOM as UTF-16BE, thus, use
        // UTF-16LE as encoding for text data.

        if (record.mediaType.endsWith(CHARSET_UTF16)) return "UTF-16LE";

        Log.w(TAG, "Unknown charset, defaulting to UTF-8.");
        return "UTF-8";
    }

    /**
     * Converts mojo NfcRecord to android.nfc.NdefRecord
     */
    private static NdefRecord toNdefRecord(NfcRecord record) throws InvalidNfcMessageException,
                                                                    IllegalArgumentException,
                                                                    UnsupportedEncodingException {
        switch (record.recordType) {
            case NfcRecordType.URL:
                return NdefRecord.createUri(new String(record.data, getCharset(record)));
            case NfcRecordType.TEXT:
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
                    return NdefRecord.createTextRecord(
                            "en-US", new String(record.data, getCharset(record)));
                } else {
                    return NdefRecord.createMime(TEXT_MIME, record.data);
                }
            case NfcRecordType.JSON:
            case NfcRecordType.OPAQUE_RECORD:
                return NdefRecord.createMime(record.mediaType, record.data);
            default:
                throw new InvalidNfcMessageException();
        }
    }
}