// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.router;

/**
 * An interface components providing media sinks and routes need to implement to hooks up into
 * {@link ChromeMediaRouter}.
 */
public interface MediaRouteProvider {
    /**
     * Initiates the discovery of media sinks corresponding to the given source id. Does nothing if
     * the source id is not supported by the MRP.
     * @param sourceId The id of the source to discover the media sinks for.
     */
    void startObservingMediaSinks(String sourceId);

    /**
     * Stops the discovery of media sinks corresponding to the given source id. Does nothing if
     * the source id is not supported by the MRP.
     * @param sourceId The id of the source to discover the media sinks for.
     */
    void stopObservingMediaSinks(String sourceId);

    /**
     * Tries to create a media route from the given media source to the media sink.
     * @param sourceId The source to create the route for.
     * @param sinkId The sink to create the route for.
     * @param nativeRequestId The id of the request tracked by the native side.
     */
    void createRoute(String sourceId, String sinkId, String routeId, int nativeRequestId);

    /**
     * Closes the media route with the given id. The route must be created by this provider.
     * @param routeId The id of the route to close.
     */
    void closeRoute(String routeId);

    /**
     * Sends a message to the route with the given id. The route must be created by this provider.
     * @param routeId The id of the route to send the message to.
     * @param message The message to send.
     * @param nativeCallbackId The id of the result callback tracked by the native side.
     */
    void sendStringMessage(String routeId, String message, int nativeCallbackId);
}
