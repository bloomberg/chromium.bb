// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import org.chromium.base.ObserverList;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.content_public.browser.MediaSession;
import org.chromium.content_public.browser.MediaSessionObserver;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.MediaMetadata;

/**
 * The MediaSessionImpl Java wrapper to allow communicating with the native MediaSessionImpl object.
 * The object is owned by Java WebContentsImpl instead of native to avoid introducing a new garbage
 * collection root.
 */
@JNINamespace("content")
public class MediaSessionImpl extends MediaSession {
    private long mNativeMediaSessionAndroid;

    private ObserverList<MediaSessionObserver> mObservers;
    private ObserverList.RewindableIterator<MediaSessionObserver> mObserversIterator;

    public static MediaSessionImpl fromWebContents(WebContents webContents) {
        return nativeGetMediaSessionFromWebContents(webContents);
    }

    public void addObserver(MediaSessionObserver observer) {
        mObservers.addObserver(observer);
    }

    public void removeObserver(MediaSessionObserver observer) {
        mObservers.removeObserver(observer);
    }

    @Override
    public ObserverList.RewindableIterator<MediaSessionObserver> getObserversForTesting() {
        return mObservers.rewindableIterator();
    }

    @Override
    public void resume() {
        nativeResume(mNativeMediaSessionAndroid);
    }

    @Override
    public void suspend() {
        nativeSuspend(mNativeMediaSessionAndroid);
    }

    @Override
    public void stop() {
        nativeStop(mNativeMediaSessionAndroid);
    }

    @CalledByNative
    private boolean hasObservers() {
        return !mObservers.isEmpty();
    }

    @CalledByNative
    private void mediaSessionDestroyed() {
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().mediaSessionDestroyed();
        }
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().stopObserving();
        }
        mObservers.clear();
        mNativeMediaSessionAndroid = 0;
    }

    @CalledByNative
    private void mediaSessionStateChanged(boolean isControllable, boolean isSuspended) {
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().mediaSessionStateChanged(isControllable, isSuspended);
        }
    }

    @CalledByNative
    private void mediaSessionMetadataChanged(MediaMetadata metadata) {
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().mediaSessionMetadataChanged(metadata);
        }
    }

    @CalledByNative
    private static MediaSessionImpl create(long nativeMediaSession) {
        return new MediaSessionImpl(nativeMediaSession);
    }

    private MediaSessionImpl(long nativeMediaSession) {
        mNativeMediaSessionAndroid = nativeMediaSession;
        mObservers = new ObserverList<MediaSessionObserver>();
        mObserversIterator = mObservers.rewindableIterator();
    }

    private native void nativeResume(long nativeMediaSessionAndroid);
    private native void nativeSuspend(long nativeMediaSessionAndroid);
    private native void nativeStop(long nativeMediaSessionAndroid);
    private static native MediaSessionImpl nativeGetMediaSessionFromWebContents(
            WebContents contents);
}
