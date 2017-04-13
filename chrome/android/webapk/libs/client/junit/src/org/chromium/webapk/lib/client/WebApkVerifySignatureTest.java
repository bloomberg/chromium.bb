// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.lib.client;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Unit tests for WebApkVerifySignature for Android. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class WebApkVerifySignatureTest {
    /** Elliptical Curves, Digital Signature Algorithm */
    private static final String KEY_FACTORY = "EC";

    private static final String TEST_DATA_DIR = "/webapks/";

    @Test
    public void testHexToBytes() throws Exception {
        byte[] empty = {};
        assertArrayEquals(empty, WebApkVerifySignature.hexToBytes(""));
        byte[] test = {(byte) 0xFF, (byte) 0xFE, 0x00, 0x01};
        assertArrayEquals(test, WebApkVerifySignature.hexToBytes("fffe0001"));
        assertArrayEquals(test, WebApkVerifySignature.hexToBytes("FFFE0001"));
        assertEquals(null, WebApkVerifySignature.hexToBytes("f")); // Odd number of nibbles.
    }

    @Test
    public void testCommentHash() throws Exception {
        byte[] bytes = {(byte) 0xde, (byte) 0xca, (byte) 0xfb, (byte) 0xad};
        assertEquals(null, WebApkVerifySignature.parseCommentSignature("weapk:decafbad"));
        assertEquals(null, WebApkVerifySignature.parseCommentSignature("webapk:"));
        assertEquals(null, WebApkVerifySignature.parseCommentSignature("webapk:decafbad"));
        assertArrayEquals(
                bytes, WebApkVerifySignature.parseCommentSignature("webapk:12345:decafbad"));
        assertArrayEquals(
                bytes, WebApkVerifySignature.parseCommentSignature("XXXwebapk:0000:decafbadXXX"));
        assertArrayEquals(
                bytes, WebApkVerifySignature.parseCommentSignature("\n\nwebapk:0000:decafbad\n\n"));
        assertArrayEquals(bytes,
                WebApkVerifySignature.parseCommentSignature("chrome-webapk:000:decafbad\n\n"));
        assertArrayEquals(bytes,
                WebApkVerifySignature.parseCommentSignature(
                        "prefixed: chrome-webapk:000:decafbad :suffixed"));
    }
}
