// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;

/** Tests for OriginVerifier. */
@RunWith(BaseJUnit4ClassRunner.class)
public class OriginVerifierTest {
    private static final byte[] BYTE_ARRAY = new byte[] {(byte) 0xaa, (byte) 0xbb, (byte) 0xcc,
            (byte) 0x10, (byte) 0x20, (byte) 0x30, (byte) 0x01, (byte) 0x02};
    private static final String STRING_ARRAY = "AA:BB:CC:10:20:30:01:02";

    private static final String PACKAGE_NAME = "org.chromium.chrome";
    private static final String SHA_256_FINGERPRINT =
            "32:A2:FC:74:D7:31:10:58:59:E5:A8:5D:F1:6D:95:F1:02:D8:5B"
            + ":22:09:9B:80:64:C5:D8:91:5C:61:DA:D1:E0";

    @Test
    @SmallTest
    public void testSHA256CertificateChecks() {
        Assert.assertEquals(OriginVerifier.byteArrayToHexString(BYTE_ARRAY), STRING_ARRAY);
        Assert.assertEquals(OriginVerifier.getCertificateSHA256FingerprintForPackage(PACKAGE_NAME),
                SHA_256_FINGERPRINT);
    }
}
