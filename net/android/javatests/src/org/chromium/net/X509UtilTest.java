// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.MediumTest;
import android.util.Base64;

import org.chromium.base.PathUtils;

import java.io.BufferedReader;
import java.io.FileReader;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.security.GeneralSecurityException;
import java.util.Arrays;

/**
 * Tests for org.chromium.net.X509Util.
 */
public class X509UtilTest extends InstrumentationTestCase {
    private static final String CERTS_DIRECTORY =
        PathUtils.getExternalStorageDirectory() + "/net/data/ssl/certificates/";
    private static final String BAD_EKU_TEST_ROOT = "eku-test-root.pem";
    private static final String CRITICAL_CODE_SIGNING_EE = "crit-codeSigning-chain.pem";
    private static final String NON_CRITICAL_CODE_SIGNING_EE = "non-crit-codeSigning-chain.pem";
    private static final String WEB_CLIENT_AUTH_EE = "invalid_key_usage_cert.der";
    private static final String OK_CERT = "ok_cert.pem";
    private static final String GOOD_ROOT_CA = "root_ca_cert.pem";

    private static final String BEGIN_MARKER = "-----BEGIN CERTIFICATE-----";
    private static final String END_MARKER = "-----END CERTIFICATE-----";

    private static byte[] pemToDer(String pemPathname) throws IOException {
        BufferedReader reader = new BufferedReader(new FileReader(pemPathname));
        StringBuilder builder = new StringBuilder();

        // Skip past leading junk lines, if any.
        String line = reader.readLine();
        while (line != null && !line.contains(BEGIN_MARKER)) line = reader.readLine();

        // Then skip the BEGIN_MARKER itself, if present.
        while (line != null && line.contains(BEGIN_MARKER)) line = reader.readLine();

        // Now gather the data lines into the builder.
        while (line != null && !line.contains(END_MARKER)) {
            builder.append(line.trim());
            line = reader.readLine();
        }

        reader.close();
        return Base64.decode(builder.toString(), Base64.DEFAULT);
    }

    private static byte[] readFileBytes(String pathname) throws IOException {
        RandomAccessFile file = new RandomAccessFile(pathname, "r");
        byte[] bytes = new byte[(int) file.length()];
        int bytesRead = file.read(bytes);
        if (bytesRead != bytes.length)
            return Arrays.copyOfRange(bytes, 0, bytesRead);
        return bytes;
    }

    @Override
    public void setUp() {
        X509Util.setDisableNativeCodeForTest(true);
    }

    @MediumTest
    public void testEkusVerified() throws GeneralSecurityException, IOException {
        X509Util.addTestRootCertificate(pemToDer(CERTS_DIRECTORY + BAD_EKU_TEST_ROOT));
        X509Util.addTestRootCertificate(pemToDer(CERTS_DIRECTORY + GOOD_ROOT_CA));

        assertFalse(X509Util.verifyKeyUsage(
            X509Util.createCertificateFromBytes(
                pemToDer(CERTS_DIRECTORY + CRITICAL_CODE_SIGNING_EE))));

        assertFalse(X509Util.verifyKeyUsage(
            X509Util.createCertificateFromBytes(
                pemToDer(CERTS_DIRECTORY + NON_CRITICAL_CODE_SIGNING_EE))));

        assertFalse(X509Util.verifyKeyUsage(
            X509Util.createCertificateFromBytes(
                readFileBytes(CERTS_DIRECTORY + WEB_CLIENT_AUTH_EE))));

        assertTrue(X509Util.verifyKeyUsage(
            X509Util.createCertificateFromBytes(
                pemToDer(CERTS_DIRECTORY + OK_CERT))));

        try {
            X509Util.clearTestRootCertificates();
        } catch (Exception e) {
            fail("Could not clear test root certificates: " + e.toString());
        }
    }
}

