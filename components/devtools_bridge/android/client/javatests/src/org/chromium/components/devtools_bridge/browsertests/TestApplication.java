// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge.browsertests;

import android.content.Context;

import org.chromium.base.CommandLine;
import org.chromium.base.PathUtils;
import org.chromium.base.ResourceExtractor;
import org.chromium.chrome.browser.ChromiumApplication;
import org.chromium.chrome.browser.PKCS11AuthenticationManager;
import org.chromium.net.AndroidPrivateKey;

import java.security.cert.X509Certificate;

/**
 * Host application for DevTools Bridge client code tests.
 */
public class TestApplication extends ChromiumApplication {
    private static final String PRIVATE_DATA_DIRECTORY_SUFFIX = "devtools_bridge";
    private static final String[] MANDATORY_PAKS = {
        "en-US.pak",
        "icudtl.dat",
        "natives_blob.bin",
        "resources.pak",
        "snapshot_blob.bin"
    };

    @Override
    public void onCreate() {
        super.onCreate();
        ResourceExtractor.setMandatoryPaksToExtract(MANDATORY_PAKS);
        PathUtils.setPrivateDataDirectorySuffix(PRIVATE_DATA_DIRECTORY_SUFFIX);
    }

    @Override
    public void initCommandLine() {
        if (!CommandLine.isInitialized()) {
            CommandLine.init(null);
        }
    }

    @Override
    protected PKCS11AuthenticationManager getPKCS11AuthenticationManager() {
        return new PKCS11AuthenticationManager() {
            @Override
            public boolean isPKCS11AuthEnabled() {
                return false;
            }

            @Override
            public String getClientCertificateAlias(String hostName, int port) {
                return null;
            }

            @Override
            public void initialize(Context context) {
            }

            @Override
            public X509Certificate[] getCertificateChain(String alias) {
                return null;
            }

            @Override
            public AndroidPrivateKey getPrivateKey(String alias) {
                return null;
            }
        };
    }

    @Override
    protected boolean areParentalControlsEnabled() {
        return false;
    }
}
