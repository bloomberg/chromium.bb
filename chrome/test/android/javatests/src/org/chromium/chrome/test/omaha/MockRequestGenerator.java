// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.omaha;

import android.content.Context;
import android.text.TextUtils;

import org.chromium.chrome.browser.omaha.RequestGenerator;
import org.xmlpull.v1.XmlSerializer;

import java.io.IOException;

/** Mocks out the RequestGenerator for tests. */
public class MockRequestGenerator extends RequestGenerator {
    public enum DeviceType {
        HANDSET, TABLET
    }

    public static final String UUID_PHONE = "uuid_phone";
    public static final String UUID_TABLET = "uuid_tablet";
    public static final String APP_ATTRIBUTE_1 = "app_attribute_1";
    public static final String APP_ATTRIBUTE_2 = "app_attribute_2";
    public static final String APP_VALUE_1 = "app_value_1";
    public static final String APP_VALUE_2 = "app_value_2";
    public static final String REQUEST_ATTRIBUTE_1 = "request_attribute_1";
    public static final String REQUEST_ATTRIBUTE_2 = "request_attribute_2";
    public static final String REQUEST_VALUE_1 = "request_value_1";
    public static final String REQUEST_VALUE_2 = "request_value_2";
    public static final String SERVER_URL = "http://totallylegitserver.com";

    private static final String BRAND = "MOCK";
    private static final String CLIENT = "mock-client";
    private static final String LANGUAGE = "zz-ZZ";
    private static final String ADDITIONAL_PARAMETERS = "chromium; manufacturer; model";

    private final boolean mIsOnTablet;

    public MockRequestGenerator(Context context, DeviceType deviceType) {
        super(context);
        mIsOnTablet = deviceType == DeviceType.TABLET;
    }

    @Override
    public String getAppId() {
        return mIsOnTablet ? UUID_TABLET : UUID_PHONE;
    }

    @Override
    public String getBrand() {
        return BRAND;
    }

    @Override
    public String getClient() {
        return CLIENT;
    }

    @Override
    public String getLanguage() {
        return LANGUAGE;
    }

    @Override
    public String getAdditionalParameters() {
        return ADDITIONAL_PARAMETERS;
    }

    @Override
    public String getServerUrl() {
        return SERVER_URL;
    }

    @Override
    protected void appendExtraAttributes(String tag, XmlSerializer serializer) throws IOException {
        if (TextUtils.equals(tag, "app")) {
            serializer.attribute(null, APP_ATTRIBUTE_1, APP_VALUE_1);
            serializer.attribute(null, APP_ATTRIBUTE_2, APP_VALUE_2);
        } else if (TextUtils.equals(tag, "request")) {
            serializer.attribute(null, REQUEST_ATTRIBUTE_1, REQUEST_VALUE_1);
            serializer.attribute(null, REQUEST_ATTRIBUTE_2, REQUEST_VALUE_2);
        }
    }

}
