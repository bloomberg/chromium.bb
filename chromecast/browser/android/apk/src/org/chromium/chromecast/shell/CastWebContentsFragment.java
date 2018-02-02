// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import android.app.Fragment;
import android.content.Context;
import android.net.Uri;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.Toast;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.content_public.browser.WebContents;

/**
 * Fragment for displaying a WebContents in CastShell.
 * <p>
 * Typically, this class is controlled by CastContentWindowAndroid through
 * CastWebContentsSurfaceHelper. If the CastContentWindowAndroid is destroyed,
 * CastWebContentsFragment should be removed from the activity holding it.
 * Similarily, if the fragment is removed from a activity or the activity holding
 * it is destroyed, CastContentWindowAndroid should be notified by intent.
 *
 * TODO(vincentli): Add a test case to test its lifecycle
 */
public class CastWebContentsFragment extends Fragment {
    private static final String TAG = "cr_CastWebContentFrg";

    private Context mPackageContext;

    private CastWebContentsSurfaceHelper mSurfaceHelper;

    private View mFragmentRootView;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        Log.d(TAG, "onCreate");
        super.onCreate(savedInstanceState);
    }

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        Log.d(TAG, "onCreateView");
        super.onCreateView(inflater, container, savedInstanceState);

        if (!CastBrowserHelper.initializeBrowser(getContext())) {
            Toast.makeText(getContext(), R.string.browser_process_initialization_failed,
                         Toast.LENGTH_SHORT)
                    .show();
            return null;
        }
        if (mFragmentRootView == null) {
            mFragmentRootView = inflater.cloneInContext(getContext())
                .inflate(R.layout.cast_web_contents_activity, null);
        }
        return mFragmentRootView;
    }


    @Override
    public Context getContext() {
        if (mPackageContext == null) {
            mPackageContext = ContextUtils.getApplicationContext();
        }
        return mPackageContext;
    }

    @Override
    public void onStart() {
        Log.d(TAG, "onStart");
        super.onStart();
        if (mSurfaceHelper != null) {
            return;
        }
        mSurfaceHelper = new CastWebContentsSurfaceHelper(getActivity(), /* hostActivity */
                    (FrameLayout) getView().findViewById(R.id.web_contents_container),
                    true /* showInFragment */);
        Bundle bundle = getArguments();
        bundle.setClassLoader(WebContents.class.getClassLoader());
        String uriString = bundle.getString(CastWebContentsComponent.INTENT_EXTRA_URI);
        if (uriString == null) {
            return;
        }
        Uri uri = Uri.parse(uriString);
        WebContents webContents = (WebContents) bundle.getParcelable(
                CastWebContentsComponent.ACTION_EXTRA_WEB_CONTENTS);

        boolean touchInputEnabled =
                bundle.getBoolean(CastWebContentsComponent.ACTION_EXTRA_TOUCH_INPUT_ENABLED, false);
        mSurfaceHelper.onNewWebContents(uri, webContents, touchInputEnabled);
    }

    @Override
    public void onPause() {
        Log.d(TAG, "onPause");
        super.onPause();
        if (mSurfaceHelper != null) {
            mSurfaceHelper.onPause();
        }
    }

    @Override
    public void onResume() {
        Log.d(TAG, "onResume");
        super.onResume();
        if (mSurfaceHelper != null) {
            mSurfaceHelper.onResume();
        }
    }

    @Override
    public void onStop() {
        Log.d(TAG, "onStop");
        super.onStop();
    }

    @Override
    public void onDestroy() {
        Log.d(TAG, "onDestroy");
        if (mSurfaceHelper != null) {
            mSurfaceHelper.onDestroy();
        }
        super.onDestroy();
    }
}
