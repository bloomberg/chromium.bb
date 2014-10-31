// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge.apiary;

import android.util.JsonReader;

import org.apache.http.HttpResponse;
import org.apache.http.client.HttpClient;
import org.apache.http.client.HttpResponseException;
import org.apache.http.client.ResponseHandler;
import org.apache.http.client.methods.HttpDelete;
import org.apache.http.client.methods.HttpEntityEnclosingRequestBase;
import org.apache.http.client.methods.HttpGet;
import org.apache.http.client.methods.HttpPost;
import org.apache.http.client.methods.HttpRequestBase;
import org.apache.http.entity.StringEntity;

import org.chromium.components.devtools_bridge.gcd.InstanceCredential;
import org.chromium.components.devtools_bridge.gcd.InstanceDescription;
import org.chromium.components.devtools_bridge.gcd.MessageReader;
import org.chromium.components.devtools_bridge.gcd.MessageWriter;

import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.net.URI;

/**
 * Client for accessing GCD API.
 */
public class GCDClient {
    private static final String API_BASE = "https://www.googleapis.com/clouddevices/v1";
    public static final String ENCODING = "UTF-8";
    protected static final String CONTENT_TYPE = "application/json; charset=" + ENCODING;

    protected final HttpClient mHttpClient;
    private final String mAPIKey;
    private final String mOAuthToken;

    GCDClient(HttpClient httpClient, String apiKey, String oAuthToken) {
        mHttpClient = httpClient;
        mAPIKey = apiKey;
        mOAuthToken = oAuthToken;
    }

    GCDClient(HttpClient httpClient, String apiKey) {
        this(httpClient, apiKey, null);
    }

    /**
     * Creation of a registration ticket is the first step in instance registration. Client must
     * have user credentials. If the ticket has been registered it will be associated with the
     * user. Next step is registration ticket patching.
     */
    public final String createRegistrationTicket() throws IOException {
        assert mOAuthToken != null;

        return mHttpClient.execute(
                newHttpPost("/registrationTickets", "{\"userEmail\":\"me\"}"),
                new JsonResponseHandler<String>() {
                    @Override
                    public String readResponse(JsonReader reader) throws IOException {
                        return new MessageReader(reader).readTicketId();
                    }
                });
    }

    /**
     * Patching registration ticket. GCD gets device definition including commands metadata,
     * GCM channel description and user-visible instance name.
     */
    public final void patchRegistrationTicket(String ticketId, InstanceDescription description)
            throws IOException {
        String content = new MessageWriter().writeTicketPatch(description).close().toString();

        mHttpClient.execute(
                newHttpPatch("/registrationTickets/" + ticketId, content),
                new EmptyResponseHandler());
    }

    /**
     * Finalizing registration. Client must be anonymous (GCD requirement). GCD provides
     * instance credentials needed for handling commands.
     */
    public final InstanceCredential finalizeRegistration(String ticketId) throws IOException {
        return mHttpClient.execute(
                newHttpPost("/registrationTickets/" + ticketId + "/finalize", ""),
                new JsonResponseHandler<InstanceCredential>() {
                    @Override
                    public InstanceCredential readResponse(JsonReader reader) throws IOException {
                        return new MessageReader(reader).readInstanceCredential();
                    }
                });
    }

    /**
     * Deletes registered instance (unregisters). If client has instance credentials then
     * instanceId must be it's own ID. If client has user credentials then instance must belong
     * to the user.
     */
    public void deleteInstance(String instanceId) throws IOException {
        mHttpClient.execute(
                newHttpDelete("/devices/" + instanceId),
                new EmptyResponseHandler());
    }

    protected final HttpGet newHttpGet(String path) {
        HttpGet request = new HttpGet(buildUrl(path));
        initializeRequest(request);
        return request;
    }

    protected final HttpPost newHttpPost(String path, String content)
            throws UnsupportedEncodingException {
        HttpPost request = new HttpPost(buildUrl(path));
        setContent(request, content);
        initializeRequest(request);
        return request;
    }

    protected final HttpPatch newHttpPatch(String path, String content)
            throws UnsupportedEncodingException {
        HttpPatch request = new HttpPatch(buildUrl(path));
        setContent(request, content);
        initializeRequest(request);
        return request;
    }

    protected final HttpDelete newHttpDelete(String path) {
        HttpDelete request = new HttpDelete(buildUrl(path));
        initializeRequest(request);
        return request;
    }

    private String buildUrl(String path) {
        return API_BASE + path + "?key=" + mAPIKey;
    }

    private void setContent(HttpEntityEnclosingRequestBase request, String content)
            throws UnsupportedEncodingException {
        request.setEntity(new StringEntity(content, ENCODING));
        request.addHeader("Content-Type", CONTENT_TYPE);
    }

    private void initializeRequest(HttpRequestBase request) {
        if (mOAuthToken != null) {
            request.addHeader("Authorization", "Bearer " + mOAuthToken);
        }
    }

    private static final class HttpPatch extends HttpEntityEnclosingRequestBase {
        public HttpPatch(String uri) {
            setURI(URI.create(uri));
        }

        public String getMethod() {
            return "PATCH";
        }
    }

    private static class EmptyResponseHandler implements ResponseHandler<Void> {
        @Override
        public Void handleResponse(HttpResponse response) throws HttpResponseException {
            JsonResponseHandler.checkStatus(response);
            return null;
        }
    }
}
