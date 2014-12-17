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

import org.chromium.components.devtools_bridge.commands.Command;
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

    public final void patchInstanceGCMChannel(String instanceId, String gcmChannelId)
            throws IOException {
        String content = new MessageWriter()
                .writeDeviceGCMChannelPatch(gcmChannelId).close().toString();

        mHttpClient.execute(
                newHttpPatch("/devices/" + instanceId, content),
                new EmptyResponseHandler());
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

    /**
     * Patches the command (previously received with Notification) with out-parameters or
     * an error message.
     */
    public final void patchCommand(Command command) throws IOException {
        String content = new MessageWriter().writeCommandPatch(command).close().toString();

        mHttpClient.execute(
                newHttpPatch("/commands/" + command.id, content),
                new EmptyResponseHandler());
    }

    protected final HttpGet newHttpGet(String path) {
        return prepare(new HttpGet(buildUrl(path)));
    }

    protected final HttpPost newHttpPost(String path, String content)
            throws UnsupportedEncodingException {
        return prepare(new HttpPost(buildUrl(path)), content);
    }

    protected final HttpPost newHttpPost(String path, String query, String content)
            throws UnsupportedEncodingException {
        return prepare(new HttpPost(buildUrl(path, query)), content);
    }

    protected final HttpPatch newHttpPatch(String path, String content)
            throws UnsupportedEncodingException {
        return prepare(new HttpPatch(buildUrl(path)), content);
    }

    protected final HttpDelete newHttpDelete(String path) {
        return prepare(new HttpDelete(buildUrl(path)));
    }

    private String buildUrl(String path) {
        return API_BASE + path + "?key=" + mAPIKey;
    }

    private String buildUrl(String path, String query) {
        return API_BASE + path + "?" + query + "&key=" + mAPIKey;
    }

    private <T extends HttpEntityEnclosingRequestBase> T prepare(T request, String content)
            throws UnsupportedEncodingException {
        request.setEntity(new StringEntity(content, ENCODING));
        request.addHeader("Content-Type", CONTENT_TYPE);
        return prepare(request);
    }

    private <T extends HttpRequestBase> T prepare(T request) {
        if (mOAuthToken != null) {
            request.addHeader("Authorization", "Bearer " + mOAuthToken);
        }
        return request;
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
