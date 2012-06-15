// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/**
 * Parameters used by ContentViewClient when a file chooser is to be
 * opened.  Java equivalent of the native
 * content/public/common/file_chooser_params.h, they should be kept in
 * sync.  See content_util.cc for conversion methods between the Java
 * and native types.
 */

public class FileChooserParams {
    // Requires that the file exists before allowing the user to pick it.
    public static final int OPEN_MODE = 0;

    // Like Open, but allows picking multiple files to open.
    public static final int OPEN_MULTIPLE_MODE = 1;

    // Like Open, but selects a folder.
    public static final int OPEN_FOLDER_MODE = 2;

    // Allows picking a nonexistent file, and prompts to overwrite if the file already exists.
    public static final int SAVE_MODE = 3;

    private int mMode;

    // Title to be used for the dialog. This may be empty for the default title, which will be
    // either "Open" or "Save" depending on the mode.
    private String mTitle;

    // Default file name to select in the dialog.
    private String mDefaultFileName;

    // A list of valid lower-cased MIME types specified in an input element. It is used to restrict
    // selectable files to such types.
    private List<String> mAcceptTypes;

    private String mCapture;

    public FileChooserParams(int mode, String title, String defaultFileName, String[] acceptTypes,
            String capture) {
        mMode = mode;
        mTitle = title;
        mDefaultFileName = defaultFileName;
        mAcceptTypes = Collections.unmodifiableList(
                new ArrayList<String>(Arrays.asList(acceptTypes)));
        mCapture = capture;
    }

    public int getMode() {
        return mMode;
    }

    public String getTitle() {
        return mTitle;
    }

    public String getDefaultFileName() {
        return mDefaultFileName;
    }

    public List<String> getAcceptTypes() {
        return mAcceptTypes;
    }

    public String getCapture() {
        return mCapture;
    }
}
