// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.os.ConditionVariable;

import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.net.impl.CronetEngineBuilderImpl;
import org.chromium.net.impl.CronetUrlRequest;

/**
 * Utilities for Cronet testing
 */
@JNINamespace("cronet")
public class CronetTestUtil {
    private static final ConditionVariable sHostResolverBlock = new ConditionVariable();

    static final String SDCH_FAKE_HOST = "fake.sdch.domain";
    // QUIC test domain must match the certificate used
    // (quic_test.example.com.crt and quic_test.example.com.key.pkcs8), and
    // the file served (
    // components/cronet/android/test/assets/test/quic_data/simple.txt).
    static final String QUIC_FAKE_HOST = "test.example.com";
    private static final String[] TEST_DOMAINS = {SDCH_FAKE_HOST, QUIC_FAKE_HOST};
    private static final String LOOPBACK_ADDRESS = "127.0.0.1";

    /**
     * Generates rules for customized DNS mapping for testing hostnames used by test servers,
     * namely:
     * <ul>
     * <li>{@link QuicTestServer#getServerHost}</li>
     * <li>{@link NativeTestServer#getSdchURL}</li>'s host
     * </ul>
     * Maps the test hostnames to 127.0.0.1.
     */
    public static JSONObject generateHostResolverRules() throws JSONException {
        return generateHostResolverRules(LOOPBACK_ADDRESS);
    }

    /**
     * Generates rules for customized DNS mapping for testing hostnames used by test servers,
     * namely:
     * <ul>
     * <li>{@link QuicTestServer#getServerHost}</li>
     * <li>{@link NativeTestServer#getSdchURL}</li>'s host
     * </ul>
     * @param destination host to map to
     */
    public static JSONObject generateHostResolverRules(String destination) throws JSONException {
        StringBuilder rules = new StringBuilder();
        for (String domain : TEST_DOMAINS) {
            rules.append("MAP " + domain + " " + destination + ",");
        }
        return new JSONObject().put("host_resolver_rules", rules);
    }

    /**
     * Returns the value of load flags in |urlRequest|.
     * @param urlRequest is the UrlRequest object of interest.
     */
    public static int getLoadFlags(UrlRequest urlRequest) {
        return nativeGetLoadFlags(((CronetUrlRequest) urlRequest).getUrlRequestAdapterForTesting());
    }

    public static void setMockCertVerifierForTesting(
            ExperimentalCronetEngine.Builder builder, long mockCertVerifier) {
        getCronetEngineBuilderImpl(builder).setMockCertVerifierForTesting(mockCertVerifier);
    }

    public static void setLibraryName(ExperimentalCronetEngine.Builder builder, String libName) {
        getCronetEngineBuilderImpl(builder).setLibraryName(libName);
    }

    public static CronetEngineBuilderImpl getCronetEngineBuilderImpl(
            ExperimentalCronetEngine.Builder builder) {
        return (CronetEngineBuilderImpl) builder.getBuilderDelegate();
    }

    private static native int nativeGetLoadFlags(long urlRequest);
}
