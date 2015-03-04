// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.text.Layout;
import android.text.Spannable;
import android.text.SpannableStringBuilder;
import android.text.style.ForegroundColorSpan;
import android.text.style.StyleSpan;
import android.util.AttributeSet;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.view.Window;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemSelectedListener;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.Spinner;
import android.widget.TextView;

import org.chromium.base.CalledByNative;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.OmniboxUrlEmphasizer;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.toolbar.ToolbarModel;
import org.chromium.chrome.browser.ui.toolbar.ToolbarModelSecurityLevel;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.base.Clipboard;
import org.chromium.ui.base.DeviceFormFactor;

import java.net.URI;
import java.net.URISyntaxException;
import java.util.Arrays;
import java.util.List;

/**
 * Java side of Android implementation of the website settings UI.
 * TODO(sashab): Rename this, and all its resources, to PageInfo* and page_info_* instead of
 *               WebsiteSettings* and website_settings_*. Do this on the C++ side as well.
 */
public class WebsiteSettingsPopup implements OnClickListener, OnItemSelectedListener {
    /**
     * An entry in the settings dropdown for a given permission. There are two options for each
     * permission: Allow and Block.
     */
    private static final class PageInfoPermissionEntry {
        public final String name;
        public final int type;
        public final int value;

        PageInfoPermissionEntry(String name, int type, int value) {
            this.name = name;
            this.type = type;
            this.value = value;
        }

        @Override
        public String toString() {
            return name;
        }
    }

    public static class ElidedUrlTextView extends TextView {
        // The number of lines to display when the URL is truncated. This number
        // should still allow the origin to be displayed. NULL before
        // setUrlAfterLayout() is called.
        private Integer mTruncatedUrlLinesToDisplay;

        // If true, the text view will show the truncated text. If false, it
        // will show the full, expanded text.
        private boolean mIsShowingTruncatedText = true;

        // The profile to use when getting the end index for the origin.
        private Profile mProfile = null;

        // The maximum number of lines currently shown in the view
        private int mCurrentMaxLines = Integer.MAX_VALUE;

        /** Constructor for inflating from XML. */
        public ElidedUrlTextView(Context context, AttributeSet attrs) {
            super(context, attrs);
        }

        @Override
        public void setMaxLines(int maxlines) {
            super.setMaxLines(maxlines);
            mCurrentMaxLines = maxlines;
        }

        @Override
        protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
            setMaxLines(Integer.MAX_VALUE);
            super.onMeasure(widthMeasureSpec, heightMeasureSpec);
            assert mProfile != null : "setProfile() must be called before layout.";

            // Lay out the URL in a StaticLayout that is the same size as our final
            // container.
            Layout layout = getLayout();
            int originEndIndex =
                    OmniboxUrlEmphasizer.getOriginEndIndex(getText().toString(), mProfile);

            // Find the range of lines containing the origin.
            int originEndLineIndex = 0;
            while (originEndLineIndex < layout.getLineCount()
                    && layout.getLineEnd(originEndLineIndex) < originEndIndex) {
                originEndLineIndex++;
            }

            // Display an extra line so we don't accidentally hide the origin with
            // ellipses
            int lastLineIndexToDisplay = originEndLineIndex + 1;

            // Since lastLineToDisplay is an index, add 1 to get the maximum number
            // of lines. This will always be at least 2 lines (when the origin is
            // fully contained on line 0).
            mTruncatedUrlLinesToDisplay = lastLineIndexToDisplay + 1;

            if (updateMaxLines()) super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        }

        /**
         * Sets the profile to use when calculating the end index of the origin.
         * Must be called before layout.
         *
         * @param profile The profile to use when coloring the URL.
         */
        public void setProfile(Profile profile) {
            mProfile = profile;
        }

        /**
         * Toggles truncating/expanding the URL text. If the URL text is not
         * truncated, has no effect.
         */
        public void toggleTruncation() {
            mIsShowingTruncatedText = !mIsShowingTruncatedText;
            updateMaxLines();
        }

        private boolean updateMaxLines() {
            int maxLines = Integer.MAX_VALUE;
            if (mIsShowingTruncatedText) maxLines = mTruncatedUrlLinesToDisplay;
            if (maxLines != mCurrentMaxLines) {
                setMaxLines(maxLines);
                return true;
            }
            return false;
        }
    }

    private static final int MAX_TABLET_DIALOG_WIDTH_DP = 400;

    private final Context mContext;
    private final Profile mProfile;
    private final WebContents mWebContents;

    // A pointer to the C++ object for this UI.
    private final long mNativeWebsiteSettingsPopup;

    // The outer container, filled with the layout from website_settings.xml.
    private final LinearLayout mContainer;

    // UI elements in the dialog.
    private final ElidedUrlTextView mUrlTitle;
    private final TextView mUrlConnectionMessage;
    private final LinearLayout mPermissionsList;
    private final Button mCopyUrlButton;
    private final Button mSiteSettingsButton;

    private final View mHorizontalSeparator;
    private final View mLowerDialogArea;

    // The dialog the container is placed in.
    private final Dialog mDialog;

    // The full URL from the URL bar, which is copied to the user's clipboard when they select 'Copy
    // URL'.
    private String mFullUrl;

    // A parsed version of mFullUrl. Is null if the URL is invalid/cannot be
    // parsed.
    private URI mParsedUrl;

    // Whether or not this page is an internal chrome page (e.g. the
    // chrome://settings page).
    private boolean mIsInternalPage;

    // The security level of the page (a valid ToolbarModelSecurityLevel).
    private int mSecurityLevel;

    /**
     * Creates the WebsiteSettingsPopup, but does not display it. Also initializes the corresponding
     * C++ object and saves a pointer to it.
     *
     * @param context Context which is used for launching a dialog.
     * @param webContents The WebContents for which to show Website information. This information is
     *                    retrieved for the visible entry.
     */
    private WebsiteSettingsPopup(Context context, Profile profile, WebContents webContents) {
        mContext = context;
        mProfile = profile;
        mWebContents = webContents;

        // Find the container and all it's important subviews.
        mContainer = (LinearLayout) LayoutInflater.from(mContext).inflate(
                R.layout.website_settings, null);

        mUrlTitle = (ElidedUrlTextView) mContainer.findViewById(R.id.website_settings_url);
        mUrlTitle.setProfile(mProfile);
        mUrlTitle.setOnClickListener(this);

        mUrlConnectionMessage = (TextView) mContainer
                .findViewById(R.id.website_settings_connection_message);
        mPermissionsList = (LinearLayout) mContainer
                .findViewById(R.id.website_settings_permissions_list);

        mCopyUrlButton = (Button) mContainer.findViewById(R.id.website_settings_copy_url_button);
        mCopyUrlButton.setOnClickListener(this);

        mSiteSettingsButton = (Button) mContainer
                .findViewById(R.id.website_settings_site_settings_button);
        mSiteSettingsButton.setOnClickListener(this);
        // Hide the Site Settings button until there's a link to take it to.
        // TODO(sashab,finnur): Make this button visible for well-formed, non-internal URLs.
        mSiteSettingsButton.setVisibility(View.GONE);

        mHorizontalSeparator = mContainer
                .findViewById(R.id.website_settings_horizontal_separator);
        mLowerDialogArea = mContainer.findViewById(R.id.website_settings_lower_dialog_area);

        // Hide the horizontal separator for sites with no permissions.
        // TODO(sashab,finnur): Show this for all sites with either the site settings button or
        // permissions (ie when the bottom area of the dialog is not empty).
        setVisibilityOfLowerDialogArea(false);

        // Create the dialog.
        mDialog = new Dialog(mContext);
        mDialog.requestWindowFeature(Window.FEATURE_NO_TITLE);
        mDialog.setCanceledOnTouchOutside(true);

        // On smaller screens, place the dialog at the top of the screen, and remove its border.
        if (!DeviceFormFactor.isTablet(mContext)) {
            Window window = mDialog.getWindow();
            window.setGravity(Gravity.TOP);
            window.setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
        }

        // This needs to come after other member initialization.
        mNativeWebsiteSettingsPopup = nativeInit(this, webContents);
        final WebContentsObserver webContentsObserver = new WebContentsObserver(mWebContents) {
            @Override
            public void navigationEntryCommitted() {
                // If a navigation is committed (e.g. from in-page redirect), the data we're showing
                // is stale so dismiss the dialog.
                mDialog.dismiss();
            }
        };
        mDialog.setOnDismissListener(new DialogInterface.OnDismissListener() {
            @Override
            public void onDismiss(DialogInterface dialog) {
                assert mNativeWebsiteSettingsPopup != 0;
                webContentsObserver.destroy();
                nativeDestroy(mNativeWebsiteSettingsPopup);
            }
        });

        // Work out the URL and connection message.
        mFullUrl = mWebContents.getVisibleUrl();
        try {
            mParsedUrl = new URI(mFullUrl);
            mIsInternalPage = UrlUtilities.isInternalScheme(mParsedUrl);
        } catch (URISyntaxException e) {
            mParsedUrl = null;
            mIsInternalPage = false;
        }
        mSecurityLevel = ToolbarModel.getSecurityLevelForWebContents(mWebContents);

        SpannableStringBuilder urlBuilder = new SpannableStringBuilder(mFullUrl);
        OmniboxUrlEmphasizer.emphasizeUrl(urlBuilder, mContext.getResources(), mProfile,
                mSecurityLevel, mIsInternalPage, true);
        mUrlTitle.setText(urlBuilder);

        // Set the URL connection message now, and the URL after layout (so it
        // can calculate its ideal height).
        mUrlConnectionMessage.setText(getUrlConnectionMessage());
    }

    /**
     * Sets the visibility of the lower area of the dialog (containing the permissions and 'Site
     * Settings' button).
     *
     * @param isVisible Whether to show or hide the dialog area.
     */
    private void setVisibilityOfLowerDialogArea(boolean isVisible) {
        mHorizontalSeparator.setVisibility(isVisible ? View.VISIBLE : View.GONE);
        mLowerDialogArea.setVisibility(isVisible ? View.VISIBLE : View.GONE);
    }

    /**
     * Finds the Image resource of the icon to use for the given permission.
     *
     * @param permission A valid ContentSettingsType that can be displayed in the PageInfo dialog to
     *                   retrieve the image for.
     * @return The resource ID of the icon to use for that permission.
     */
    private int getImageResourceForPermission(int permission) {
        switch (permission) {
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_IMAGES:
                return R.drawable.permission_images;
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_JAVASCRIPT:
                return R.drawable.permission_javascript;
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_GEOLOCATION:
                return R.drawable.permission_location;
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA:
                return R.drawable.permission_media;
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC:
                return R.drawable.permission_mic;
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_NOTIFICATIONS:
                return R.drawable.permission_push_notification;
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_POPUPS:
                return R.drawable.permission_popups;
            default:
                assert false : "Icon requested for invalid permission: " + permission;
                return -1;
        }
    }

    /**
     * Gets the message to display in the connection message box for the given security level. Does
     * not apply to SECURITY_ERROR pages, since these have their own coloured/formatted message.
     *
     * @param toolbarModelSecurityLevel A valid ToolbarModelSecurityLevel, which is the security
     *                                  level of the page.
     * @param isInternalPage Whether or not this page is an internal chrome page (e.g. the
     *                       chrome://settings page).
     * @return The ID of the message to display in the connection message box.
     */
    private int getConnectionMessageId(int toolbarModelSecurityLevel, boolean isInternalPage) {
        if (isInternalPage) return R.string.page_info_connection_internal_page;

        switch (toolbarModelSecurityLevel) {
            case ToolbarModelSecurityLevel.NONE:
                return R.string.page_info_connection_http;
            case ToolbarModelSecurityLevel.SECURE:
            case ToolbarModelSecurityLevel.EV_SECURE:
                return R.string.page_info_connection_https;
            case ToolbarModelSecurityLevel.SECURITY_WARNING:
            case ToolbarModelSecurityLevel.SECURITY_POLICY_WARNING:
                return R.string.page_info_connection_mixed;
            default:
                assert false : "Invalid security level specified: " + toolbarModelSecurityLevel;
                return R.string.page_info_connection_http;
        }
    }

    /**
     * Gets the styled connection message to display below the URL.
     */
    private Spannable getUrlConnectionMessage() {
        // Display the appropriate connection message.
        SpannableStringBuilder messageBuilder = new SpannableStringBuilder();
        if (mSecurityLevel != ToolbarModelSecurityLevel.SECURITY_ERROR) {
            messageBuilder.append(mContext.getResources().getString(
                    getConnectionMessageId(mSecurityLevel, mIsInternalPage)));
        } else {
            String originToDisplay;
            try {
                URI parsedUrl = new URI(mFullUrl);
                originToDisplay = UrlUtilities.getOriginForDisplay(parsedUrl, false);
            } catch (URISyntaxException e) {
                // The URL is invalid - just display the full URL.
                originToDisplay = mFullUrl;
            }

            String leadingText = mContext.getResources().getString(
                    R.string.page_info_connection_broken_leading_text);
            String followingText = mContext.getResources().getString(
                    R.string.page_info_connection_broken_following_text, originToDisplay);
            messageBuilder.append(leadingText + " " + followingText);
            final ForegroundColorSpan redSpan = new ForegroundColorSpan(mContext.getResources()
                    .getColor(R.color.website_settings_connection_broken_leading_text));
            final StyleSpan boldSpan = new StyleSpan(android.graphics.Typeface.BOLD);
            messageBuilder.setSpan(redSpan, 0, leadingText.length(),
                    Spannable.SPAN_INCLUSIVE_EXCLUSIVE);
            messageBuilder.setSpan(boldSpan, 0, leadingText.length(),
                    Spannable.SPAN_INCLUSIVE_EXCLUSIVE);
        }
        return messageBuilder;
    }

    /**
     * Adds a new row for the given permission.
     *
     * @param name The title of the permission to display to the user.
     * @param type The ContentSettingsType of the permission.
     * @param currentSetting The ContentSetting of the currently selected setting.
     */
    @CalledByNative
    private void addPermissionSection(String name, int type, int currentSetting) {
        // We have at least one permission, so show the lower permissions area.
        setVisibilityOfLowerDialogArea(true);

        View permissionRow = LayoutInflater.from(mContext).inflate(
                R.layout.website_settings_permission_row, null);

        ImageView permission_icon = (ImageView) permissionRow.findViewById(
                R.id.website_settings_permission_icon);
        permission_icon.setImageResource(getImageResourceForPermission(type));

        TextView permission_name = (TextView) permissionRow.findViewById(
                R.id.website_settings_permission_name);
        permission_name.setText(name);

        Spinner permission_spinner = (Spinner) permissionRow.findViewById(
                R.id.website_settings_permission_spinner);

        // Work out the index of the currently selected setting.
        int selectedSettingIndex = -1;
        switch (currentSetting) {
            case ContentSetting.ALLOW:
                selectedSettingIndex = 0;
                break;
            case ContentSetting.BLOCK:
                selectedSettingIndex = 1;
                break;
            default:
                assert false : "Invalid setting " + currentSetting + " for permission " + type;
        }

        List<PageInfoPermissionEntry> settingsChoices = Arrays.asList(
                new PageInfoPermissionEntry(mContext.getResources().getString(
                        R.string.page_info_permission_allow), type, ContentSetting.ALLOW),
                new PageInfoPermissionEntry(mContext.getResources().getString(
                        R.string.page_info_permission_block), type, ContentSetting.BLOCK));
        ArrayAdapter<PageInfoPermissionEntry> adapter = new ArrayAdapter<PageInfoPermissionEntry>(
                mContext, R.drawable.website_settings_permission_spinner_item, settingsChoices);
        adapter.setDropDownViewResource(
                R.drawable.website_settings_permission_spinner_dropdown_item);
        permission_spinner.setAdapter(adapter);
        permission_spinner.setSelection(selectedSettingIndex, false);
        permission_spinner.setOnItemSelectedListener(this);
        mPermissionsList.addView(permissionRow);
    }

    /**
     * Displays the WebsiteSettingsPopup.
     */
    @CalledByNative
    private void showDialog() {
        if (!DeviceFormFactor.isTablet(mContext)) {
            // On smaller screens, make the dialog fill the width of the screen.
            ScrollView scrollView = new ScrollView(mContext);
            scrollView.addView(mContainer);
            mDialog.addContentView(scrollView, new LinearLayout.LayoutParams(
                    LinearLayout.LayoutParams.MATCH_PARENT,
                    LinearLayout.LayoutParams.MATCH_PARENT));

            // This must be called after addContentView, or it won't fully fill to the edge.
            Window window = mDialog.getWindow();
            window.setLayout(ViewGroup.LayoutParams.MATCH_PARENT,
                    ViewGroup.LayoutParams.WRAP_CONTENT);
        } else {
            // On larger screens, make the dialog centered in the screen and have a maximum width.
            ScrollView scrollView = new ScrollView(mContext) {
                @Override
                protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
                    final int maxDialogWidthInPx = (int) (MAX_TABLET_DIALOG_WIDTH_DP
                            * mContext.getResources().getDisplayMetrics().density);
                    if (MeasureSpec.getSize(widthMeasureSpec) > maxDialogWidthInPx) {
                        widthMeasureSpec = MeasureSpec.makeMeasureSpec(maxDialogWidthInPx,
                                MeasureSpec.EXACTLY);
                    }
                    super.onMeasure(widthMeasureSpec, heightMeasureSpec);
                }
            };

            scrollView.addView(mContainer);
            mDialog.addContentView(scrollView, new LinearLayout.LayoutParams(
                    LinearLayout.LayoutParams.WRAP_CONTENT,
                    LinearLayout.LayoutParams.MATCH_PARENT));
        }

        mDialog.show();
    }

    @Override
    public void onItemSelected(AdapterView<?> parent, View view, int pos, long id) {
        PageInfoPermissionEntry entry = (PageInfoPermissionEntry) parent.getItemAtPosition(pos);
        nativeOnPermissionSettingChanged(mNativeWebsiteSettingsPopup, entry.type, entry.value);
    }

    @Override
    public void onNothingSelected(AdapterView<?> parent) {
        // Do nothing intentionally.
    }

    @Override
    public void onClick(View view) {
        if (view == mCopyUrlButton) {
            new Clipboard(mContext).setText(mFullUrl, mFullUrl);
            mDialog.dismiss();
        } else if (view == mSiteSettingsButton) {
            // TODO(sashab,finnur): Make this open the Website Settings dialog.
            assert false : "No Website Settings here!";
            mDialog.dismiss();
        } else if (view == mUrlTitle) {
            // Expand/collapse the displayed URL title.
            mUrlTitle.toggleTruncation();
        }
    }

    /**
     * Shows a WebsiteSettings dialog for the provided WebContents. The popup adds itself to the
     * view hierarchy which owns the reference while it's visible.
     *
     * @param context Context which is used for launching a dialog.
     * @param webContents The WebContents for which to show Website information. This information is
     *                    retrieved for the visible entry.
     */
    @SuppressWarnings("unused")
    public static void show(Context context, Profile profile, WebContents webContents) {
        new WebsiteSettingsPopup(context, profile, webContents);
    }

    private static native long nativeInit(WebsiteSettingsPopup popup, WebContents webContents);

    private native void nativeDestroy(long nativeWebsiteSettingsPopupAndroid);

    private native void nativeOnPermissionSettingChanged(long nativeWebsiteSettingsPopupAndroid,
            int type, int setting);
}
