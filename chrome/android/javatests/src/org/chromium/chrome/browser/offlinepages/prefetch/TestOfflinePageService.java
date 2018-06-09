// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages.prefetch;

import com.google.protobuf.InvalidProtocolBufferException;

import org.chromium.base.Log;
import org.chromium.components.offline_pages.core.prefetch.proto.AnyOuterClass.Any;
import org.chromium.components.offline_pages.core.prefetch.proto.OfflinePages.Archive;
import org.chromium.components.offline_pages.core.prefetch.proto.OfflinePages.GeneratePageBundleRequest;
import org.chromium.components.offline_pages.core.prefetch.proto.OfflinePages.PageBundle;
import org.chromium.components.offline_pages.core.prefetch.proto.OfflinePages.PageInfo;
import org.chromium.components.offline_pages.core.prefetch.proto.OfflinePages.PageParameters;
import org.chromium.components.offline_pages.core.prefetch.proto.OperationOuterClass.Operation;
import org.chromium.components.offline_pages.core.prefetch.proto.StatusOuterClass;
import org.chromium.net.test.util.WebServer;

import java.io.OutputStream;

/**
 * A fake OfflinePageService.
 *
 * Currently this implements static happy-path behavior, but it should be
 * extended to support other behaviors as necessary.
 */
public class TestOfflinePageService {
    public static final int BODY_LENGTH = 1000;
    private static final String TAG = "TestOPS";
    private static final String BODY_PREFIX = "body-name-";

    /** Handle a request. */
    public void handleRequest(WebServer.HTTPRequest request, OutputStream stream)
            throws java.io.IOException {
        if (request.getURI().startsWith("/v1:GeneratePageBundle")) {
            GeneratePageBundleRequest bundleRequest = null;
            try {
                bundleRequest = GeneratePageBundleRequest.parseFrom(request.getBody());
                handleGeneratePageBundle(bundleRequest, stream);
                return;
            } catch (InvalidProtocolBufferException e) {
                Log.e(TAG, "InvalidProtocolBufferException " + e.getMessage());
            }
        }
        if (request.getURI().startsWith("/v1/media/")) {
            String suffix = request.getURI().substring(10);
            String[] nameAndQuery = suffix.split("[?]", 2);
            if (nameAndQuery.length == 2 && handleRead(nameAndQuery[0], stream)) return;
        }
        Log.e(TAG, "Unprocessed request: " + request.getURI());
    }

    /** @return test content for the body with the given name. */
    static byte[] bodyContent(String bodyName) {
        // The content is just the body name repeated until BODY_LENGTH is reached.
        StringBuilder body = new StringBuilder();
        while (body.length() < BODY_LENGTH) {
            body.append(bodyName);
        }
        body.setLength(BODY_LENGTH);
        return body.toString().getBytes();
    }

    /** Handle a Read request by returning fake MHTML content. */
    private boolean handleRead(String bodyName, OutputStream output) throws java.io.IOException {
        // Sanity check that bodyName looks like a body name.
        if (!bodyName.startsWith(BODY_PREFIX)) {
            return false;
        }
        WebServer.writeResponse(output, WebServer.STATUS_OK, bodyContent(bodyName));
        return true;
    }

    /**
     * Handle a GeneratePageBundle request.
     *
     * Respond with a 'complete' bundle, where each page is a fixed size.
     */
    private void handleGeneratePageBundle(GeneratePageBundleRequest request, OutputStream output)
            throws java.io.IOException {
        PageBundle.Builder bundle = PageBundle.newBuilder();

        for (int i = 0; i < request.getPagesCount(); i++) {
            PageParameters params = request.getPages(i);
            String bodyName = BODY_PREFIX + String.valueOf(i);
            bundle.addArchives(
                    Archive.newBuilder()
                            .setBodyName(bodyName)
                            .setBodyLength(BODY_LENGTH)
                            .addPageInfos(
                                    PageInfo.newBuilder()
                                            .setUrl(params.getUrl())
                                            .setStatus(StatusOuterClass.Status.newBuilder().setCode(
                                                    StatusOuterClass.Code.OK_VALUE)))
                            .build());
        }
        Any anyBundle = Any.newBuilder()
                                .setTypeUrl("type.googleapis.com/"
                                        + "google.internal.chrome.offlinepages.v1.PageBundle")
                                .setValue(bundle.build().toByteString())
                                .build();
        Operation response = Operation.newBuilder()
                                     .setName("operation-1")
                                     .setDone(true)
                                     .setResponse(anyBundle)
                                     .build();

        WebServer.writeResponse(output, WebServer.STATUS_OK, response.toByteArray());
    }
}
