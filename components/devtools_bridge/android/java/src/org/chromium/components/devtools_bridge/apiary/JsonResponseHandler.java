// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge.apiary;

import android.util.JsonReader;

import org.apache.http.HttpEntity;
import org.apache.http.HttpResponse;
import org.apache.http.StatusLine;
import org.apache.http.client.ClientProtocolException;
import org.apache.http.client.HttpResponseException;
import org.apache.http.client.ResponseHandler;

import java.io.IOException;
import java.io.InputStreamReader;

/**
 * Base class for a ResponseHandler that reads response with JsonReader. Like BasicResponseHandler
 * throws HttpResponseException if response code >= 300.
 *
 * It catchs JsonReader's runtime exception (IllegalStateException and IllegalArgumentException)
 * and wraps them into ResponseFormatException.
 */
abstract class JsonResponseHandler<T> implements ResponseHandler<T> {
    public static void checkStatus(HttpResponse response) throws HttpResponseException {
        StatusLine statusLine = response.getStatusLine();
        if (response.getStatusLine().getStatusCode() >= 300) {
            throw new HttpResponseException(
                    statusLine.getStatusCode(), statusLine.getReasonPhrase());
        }
    }

    @Override
    public final T handleResponse(HttpResponse response)
            throws IOException, ClientProtocolException {
        checkStatus(response);
        HttpEntity entity = response.getEntity();
        if (entity == null) {
            throw new ClientProtocolException("Missing content");
        }
        JsonReader reader = new JsonReader(new InputStreamReader(entity.getContent()));
        try {
            T result = readResponse(reader);
            reader.close();

            if (result == null) {
                throw new ClientProtocolException("Missing result");
            }

            return result;
        } catch (IllegalStateException e) {
            throw new ResponseFormatException(e);
        } catch (IllegalArgumentException e) {
            throw new ResponseFormatException(e);
        }
    }

    public abstract T readResponse(JsonReader reader)
            throws IOException, ResponseFormatException;

    public static class ResponseFormatException extends ClientProtocolException {
        public ResponseFormatException(RuntimeException readerException) {
            super(readerException);
        }

        public ResponseFormatException() {}
    }
}
