// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.urlconnection;

import org.chromium.net.ExtendedResponseInfo;
import org.chromium.net.ResponseInfo;
import org.chromium.net.UrlRequest;
import org.chromium.net.UrlRequestContext;
import org.chromium.net.UrlRequestException;
import org.chromium.net.UrlRequestListener;

import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.URL;
import java.nio.ByteBuffer;

/**
 * An implementation of HttpURLConnection that uses Cronet to send requests and
 * receive response. This class inherits a {@code connected} field from the
 * superclass. That field indicates whether a connection has ever been
 * attempted.
 */
public class CronetHttpURLConnection extends HttpURLConnection {
    private final UrlRequestContext mUrlRequestContext;
    private final MessageLoop mMessageLoop;
    private final UrlRequest mRequest;

    private CronetInputStream mInputStream;
    private ResponseInfo mResponseInfo;
    private ByteBuffer mResponseByteBuffer;
    private boolean mOnRedirectCalled = false;

    protected CronetHttpURLConnection(URL url,
            UrlRequestContext urlRequestContext) {
        super(url);
        mUrlRequestContext = urlRequestContext;
        mMessageLoop = new MessageLoop();
        mRequest = mUrlRequestContext.createRequest(url.toString(),
                new CronetUrlRequestListener(), mMessageLoop);
        mInputStream = new CronetInputStream(this);
    }

    /**
     * Opens a connection to the resource. If the connect method is called when
     * the connection has already been opened (indicated by the connected field
     * having the value true), the call is ignored.
     */
    @Override
    public void connect() throws IOException {
        if (connected) {
            return;
        }
        connected = true;
        mRequest.start();
        // Blocks until onResponseStarted or onFailed is called.
        mMessageLoop.loop();
    }

    /**
     * Releases this connection so that its resources may be either reused or
     * closed.
     */
    @Override
    public void disconnect() {
        // Disconnect before connection is made should have no effect.
        if (connected) {
            try {
                mInputStream.close();
            } catch (IOException e) {
                e.printStackTrace();
            }
            mInputStream = null;
            mRequest.cancel();
        }
    }

    /**
     * Returns the response message returned by the remote HTTP server.
     */
    @Override
    public String getResponseMessage() throws IOException {
        connect();
        if (mResponseInfo == null) {
            return "";
        }
        return mResponseInfo.getHttpStatusText();
    }

    /**
     * Returns the response code returned by the remote HTTP server.
     */
    @Override
    public int getResponseCode() throws IOException {
        connect();
        if (mResponseInfo == null) {
            return 0;
        }
        return mResponseInfo.getHttpStatusCode();
    }

    /**
     * Returns an InputStream for reading data from the resource pointed by this
     * URLConnection.
     */
    @Override
    public InputStream getInputStream() throws IOException {
        connect();
        if (mResponseInfo == null) {
            throw new IOException();
        }
        if (!instanceFollowRedirects && mOnRedirectCalled) {
            throw new IOException("Cannot read response body of a redirect.");
        }
        return mInputStream;
    }

    /**
     * Adds the given property to the request header.
     */
    @Override
    public final void addRequestProperty(String key, String value) {
        // Note that Cronet right now does not allow setting multiple headers
        // of the same key, see crbug.com/432719 for more details.
        setRequestProperty(key, value);
    }

    /**
     * Sets the value of the specified request header field.
     */
    @Override
    public final void setRequestProperty(String key, String value) {
        if (connected) {
            throw new IllegalStateException(
                    "Cannot set request property after connection is made");
        }
        mRequest.addHeader(key, value);
    }

    /**
     * Returns whether this connection uses a proxy server.
     */
    @Override
    public boolean usingProxy() {
        // TODO(xunjieli): implement this.
        return false;
    }

    /**
     * Used by {@link CronetInputStream} to get more data from the network
     * stack. This should only be called after the request has started. Note
     * that this call might block if there isn't any more data to be read.
     */
    ByteBuffer getMoreData() throws IOException {
        mResponseByteBuffer = null;
        mMessageLoop.loop();
        return mResponseByteBuffer;
    }

    private class CronetUrlRequestListener implements UrlRequestListener {
        public CronetUrlRequestListener() {
        }

        @Override
        public void onResponseStarted(UrlRequest request, ResponseInfo info) {
            mResponseInfo = info;
            // Quits the message loop since we have the headers now.
            mMessageLoop.postQuitTask();
        }

        @Override
        public void onDataReceived(UrlRequest request, ResponseInfo info,
                ByteBuffer byteBuffer) {
            mResponseInfo = info;
            mResponseByteBuffer = ByteBuffer.allocate(byteBuffer.capacity());
            mResponseByteBuffer.put(byteBuffer);
            mResponseByteBuffer.flip();
            mMessageLoop.postQuitTask();
        }

        @Override
        public void onRedirect(UrlRequest request, ResponseInfo info,
                String newLocationUrl) {
            mOnRedirectCalled = true;
            if (instanceFollowRedirects) {
                try {
                    url = new URL(newLocationUrl);
                } catch (MalformedURLException e) {
                    // Ignored.
                }
            } else {
                mResponseInfo = info;
                mRequest.cancel();
                setResponseDataCompleted();
            }
        }

        @Override
        public void onSucceeded(UrlRequest request, ExtendedResponseInfo info) {
            mResponseInfo = info.getResponseInfo();
            setResponseDataCompleted();
        }

        @Override
        public void onFailed(UrlRequest request, ResponseInfo info,
                UrlRequestException exception) {
            // TODO(xunjieli): Handle failure.
            setResponseDataCompleted();
        }

        /**
         * Notifies {@link #mInputStream} that transferring of response data has
         * completed.
         */
        private void setResponseDataCompleted() {
            if (mInputStream != null) {
                mInputStream.setResponseDataCompleted();
            }
            mMessageLoop.postQuitTask();
        }
    }
}
