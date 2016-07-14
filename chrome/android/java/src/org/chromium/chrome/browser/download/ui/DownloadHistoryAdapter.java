// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.ui;

import android.content.Context;
import android.support.v7.widget.RecyclerView;
import android.text.TextUtils;
import android.text.format.Formatter;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.download.DownloadItem;
import org.chromium.chrome.browser.util.UrlUtilities;

import java.util.ArrayList;
import java.util.List;

/** Bridges the user's download history and the UI used to display it. */
public class DownloadHistoryAdapter
        extends RecyclerView.Adapter<DownloadHistoryAdapter.ViewHolder> {

    /** Holds onto a View that displays information about a downloaded file. */
    static class ViewHolder extends RecyclerView.ViewHolder {
        public ImageView mIconView;
        public TextView mFilenameView;
        public TextView mHostnameView;
        public TextView mFilesizeView;

        public ViewHolder(View itemView) {
            super(itemView);
            mIconView = (ImageView) itemView.findViewById(R.id.icon_view);
            mFilenameView = (TextView) itemView.findViewById(R.id.filename_view);
            mHostnameView = (TextView) itemView.findViewById(R.id.hostname_view);
            mFilesizeView = (TextView) itemView.findViewById(R.id.filesize_view);
        }
    }

    private static final int FILETYPE_OTHER = 0;
    private static final int FILETYPE_PAGE = 1;
    private static final int FILETYPE_VIDEO = 2;
    private static final int FILETYPE_AUDIO = 3;
    private static final int FILETYPE_IMAGE = 4;
    private static final int FILETYPE_DOCUMENT = 5;

    private static final String MIMETYPE_VIDEO = "video";
    private static final String MIMETYPE_AUDIO = "audio";
    private static final String MIMETYPE_IMAGE = "image";
    private static final String MIMETYPE_DOCUMENT = "text";

    private final List<DownloadItem> mDownloadItems = new ArrayList<>();

    /** Called when the user's download history has been gathered into a List of DownloadItems. */
    public void onAllDownloadsRetrieved(List<DownloadItem> list) {
        mDownloadItems.clear();
        for (DownloadItem item : list) mDownloadItems.add(item);
        notifyDataSetChanged();
    }

    @Override
    public ViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
        View v = LayoutInflater.from(parent.getContext()).inflate(
                R.layout.download_item_view, parent, false);
        return new ViewHolder(v);
    }

    @Override
    public void onBindViewHolder(ViewHolder holder, int position) {
        Context context = holder.mFilesizeView.getContext();

        DownloadItem item = mDownloadItems.get(position);
        holder.mFilenameView.setText(item.getDownloadInfo().getFileName());
        holder.mHostnameView.setText(
                UrlUtilities.formatUrlForSecurityDisplay(item.getDownloadInfo().getUrl(), false));
        holder.mFilesizeView.setText(
                Formatter.formatFileSize(context, item.getDownloadInfo().getContentLength()));

        // Pick what icon to display for the item.
        int fileType = convertMimeTypeToFileType(item.getDownloadInfo().getMimeType());
        int iconResource = R.drawable.ic_drive_file_white_24dp;
        switch (fileType) {
            case FILETYPE_PAGE:
                iconResource = R.drawable.ic_drive_site_white_24dp;
                break;
            case FILETYPE_VIDEO:
                iconResource = R.drawable.ic_music_video_white_24dp;
                break;
            case FILETYPE_AUDIO:
                iconResource = R.drawable.ic_music_note_white_24dp;
                break;
            case FILETYPE_IMAGE:
                iconResource = R.drawable.ic_image_white_24dp;
                break;
            case FILETYPE_DOCUMENT:
                iconResource = R.drawable.ic_drive_file_white_24dp;
                break;
            default:
        }

        holder.mIconView.setImageResource(iconResource);
    }

    @Override
    public int getItemCount() {
        return mDownloadItems.size();
    }

    /** Clears out all of the downloads tracked by the Adapter. */
    public void clear() {
        mDownloadItems.clear();
    }

    /** Identifies the type of file represented by the given MIME type string. */
    private static int convertMimeTypeToFileType(String mimeType) {
        if (TextUtils.isEmpty(mimeType)) return FILETYPE_OTHER;

        String[] pieces = mimeType.toLowerCase().split("/");
        if (pieces.length != 2) return FILETYPE_OTHER;

        if (MIMETYPE_VIDEO.equals(pieces[0])) {
            return FILETYPE_VIDEO;
        } else if (MIMETYPE_AUDIO.equals(pieces[0])) {
            return FILETYPE_AUDIO;
        } else if (MIMETYPE_IMAGE.equals(pieces[0])) {
            return FILETYPE_IMAGE;
        } else if (MIMETYPE_DOCUMENT.equals(pieces[0])) {
            return FILETYPE_DOCUMENT;
        } else {
            return FILETYPE_OTHER;
        }
    }
}
