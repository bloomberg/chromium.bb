// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge.apiary;

import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;
import android.util.JsonReader;

import junit.framework.Assert;

import org.apache.http.ProtocolVersion;
import org.apache.http.client.ClientProtocolException;
import org.apache.http.client.HttpResponseException;
import org.apache.http.entity.StringEntity;
import org.apache.http.message.BasicHttpResponse;

import java.io.IOException;

/**
 * Tests for {@link JsonResponseHandler}.
 */
public class JsonResponseHandlerTest extends InstrumentationTestCase {
    @SmallTest
    public void testInvalidResponse() throws IOException {
        try {
            new JsonResponseHandler<String>() {
                        @Override
                        public String readResponse(JsonReader reader) {
                            return "";
                        }
                    }.handleResponse(newResponse(404, null));

            Assert.fail();
        } catch (HttpResponseException e) {
            // Expected
        }
    }

    @SmallTest
    public void testIOException() {
        try {
            new JsonResponseHandler<String>() {
                        @Override
                        public String readResponse(JsonReader reader) throws IOException {
                            throw new IOException();
                        }
                    }.handleResponse(newResponse(200, ""));

            Assert.fail();
        } catch (IOException e) {
            // Expected
        }
    }

    @SmallTest
    public void testStateException() throws IOException {
        try {
            new JsonResponseHandler<String>() {
                        @Override
                        public String readResponse(JsonReader reader) throws IOException {
                            reader.beginObject();
                            reader.endObject();
                            return "";
                        }
                    }.handleResponse(newResponse(200, "[]"));

            Assert.fail();
        } catch (JsonResponseHandler.ResponseFormatException e) {
            // Expected
        }
    }

    @SmallTest
    public void testFormatException() throws IOException {
        try {
            new JsonResponseHandler<String>() {
                        @Override
                        public String readResponse(JsonReader reader) throws IOException {
                            reader.beginArray();
                            reader.nextLong();
                            reader.endArray();
                            return "";
                        }
                    }.handleResponse(newResponse(200, "[\"XXX\"]"));

            Assert.fail();
        } catch (JsonResponseHandler.ResponseFormatException e) {
            // Expected
        }
    }

    @SmallTest
    public void testNullResultException() throws IOException {
        try {
            new JsonResponseHandler<String>() {
                        @Override
                        public String readResponse(JsonReader reader) throws IOException {
                            reader.beginArray();
                            reader.endArray();
                            return null;
                        }
                    }.handleResponse(newResponse(200, "[]"));

            Assert.fail();
        } catch (ClientProtocolException e) {
            // Expected
        }
    }

    @SmallTest
    public void testSuccess() throws IOException {
        String result = new JsonResponseHandler<String>() {
                    @Override
                    public String readResponse(JsonReader reader) throws IOException {
                        reader.beginArray();
                        reader.endArray();
                        return "OK";
                    }
                }.handleResponse(newResponse(200, "[]"));

        Assert.assertEquals("OK", result);
    }

    private BasicHttpResponse newResponse(int status, String content) {
        BasicHttpResponse response = new BasicHttpResponse(
                new ProtocolVersion("HTTP", 1, 1), status, "reason");
        if (content != null) {
            try {
                response.setEntity(new StringEntity(content, "UTF-8"));
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }
        return response;
    }
}
