// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.common;

import android.support.annotation.NonNull;
import android.text.TextUtils;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * The MediaMetadata class carries information related to a media session. It is
 * the Java counterpart of content::MediaMetadata.
 */
@JNINamespace("content")
public class MediaMetadata {

    @NonNull
    private String mTitle;

    @NonNull
    private String mArtist;

    @NonNull
    private String mAlbum;

    /**
     * Returns the title associated with the media session.
     */
    public String getTitle() {
        return mTitle;
    }

    /**
     * Returns the artist name associated with the media session.
     */
    public String getArtist() {
        return mArtist;
    }

    /**
     * Returns the album name associated with the media session.
     */
    public String getAlbum() {
        return mAlbum;
    }

    /**
     * Sets the title associated with the media session.
     * @param title The title to use for the media session.
     */
    public void setTitle(String title) {
        mTitle = title;
    }

    /**
     * Sets the arstist name associated with the media session.
     * @param arstist The artist name to use for the media session.
     */
    public void setArtist(String artist) {
        mArtist = artist;
    }

    /**
     * Sets the album name associated with the media session.
     * @param album The album name to use for the media session.
     */
    public void setAlbum(String album) {
        mAlbum = album;
    }

    /**
     * Creates a new MediaMetadata from the C++ code. This is exactly like the
     * constructor below apart that it can be called by native code.
     */
    @CalledByNative
    private static MediaMetadata create(String title, String artist, String album) {
        return new MediaMetadata(title == null ? "" : title, artist == null ? "" : artist,
            album == null ? "" : album);
    }

    /**
     * Creates a new MediaMetadata.
     */
    public MediaMetadata(@NonNull String title, @NonNull String artist, @NonNull String album) {
        mTitle = title;
        mArtist = artist;
        mAlbum = album;
    }

    /**
     * Copy constructor.
     */
    public MediaMetadata(MediaMetadata other) {
        this(other.mTitle, other.mArtist, other.mAlbum);
    }

    @Override
    public boolean equals(Object obj) {
        if (obj == this) return true;
        if (!(obj instanceof MediaMetadata)) return false;

        MediaMetadata other = (MediaMetadata) obj;
        return TextUtils.equals(mTitle, other.mTitle)
                && TextUtils.equals(mArtist, other.mArtist)
                && TextUtils.equals(mAlbum, other.mAlbum);
    }

    @Override
    public int hashCode() {
        int result = mTitle.hashCode();
        result = 31 * result + mArtist.hashCode();
        result = 31 * result + mAlbum.hashCode();
        return result;
    }
}
