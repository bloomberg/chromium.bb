// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.common;

import android.graphics.Rect;
import android.support.annotation.NonNull;
import android.text.TextUtils;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

import java.util.ArrayList;
import java.util.List;

/**
 * The MediaMetadata class carries information related to a media session. It is
 * the Java counterpart of content::MediaMetadata.
 */
@JNINamespace("content")
public class MediaMetadata {
    /**
     * The Artwork class carries the artwork information in MediaMetadata. It is the Java
     * counterpart of content::MediaMetadata::Artwork.
     */
    public static class Artwork {
        @NonNull
        private String mSrc;

        private String mType;

        @NonNull
        private List<Rect> mSizes = new ArrayList<Rect>();

        /**
         * Creates a new Artwork.
         */
        public Artwork(@NonNull String src, String type, List<Rect> sizes) {
            mSrc = src;
            mType = type;
            mSizes = sizes;
        }

        /**
         * Returns the URL of this Artwork.
         */
        @NonNull
        public String getSrc() {
            return mSrc;
        }

        /**
         * Returns the MIME type of this Artwork.
         */
        public String getType() {
            return mType;
        }

        /**
         * Returns the hinted sizes of this Artwork.
         */
        public List<Rect> getSizes() {
            return mSizes;
        }

        /**
         * Sets the URL of this Artwork.
         */
        public void setSrc(String src) {
            mSrc = src;
        }

        /**
         * Sets the MIME type of this Artwork.
         */
        public void setType(String type) {
            mType = type;
        }

        /**
         * Sets the sizes of this Artwork.
         */
        public void setSizes(List<Rect> sizes) {
            mSizes = sizes;
        }
    }

    @NonNull
    private String mTitle;

    @NonNull
    private String mArtist;

    @NonNull
    private String mAlbum;

    @NonNull
    private List<Artwork> mArtwork = new ArrayList<Artwork>();

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

    public List<Artwork> getArtwork() {
        return mArtwork;
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
     * Create a new MediaArtwork from the C++ code, and add it to the Metadata.
     * @param src The URL of the artwork.
     * @param type The MIME type of the artwork.
     * @param flattenedSizes The flattened array of Artwork sizes. In native code, it is of type
     *         `std::vector<gfx::Size>` before flattening.
     */
    @CalledByNative
    private void createAndAddArtwork(String src, String type, int[] flattenedSizes) {
        assert (flattenedSizes.length % 2) == 0;
        List<Rect> sizes = new ArrayList<Rect>();
        for (int i = 0; (i + 1) < flattenedSizes.length; i += 2) {
            sizes.add(new Rect(0, 0, flattenedSizes[i], flattenedSizes[i + 1]));
        }
        mArtwork.add(new Artwork(src, type, sizes));
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
