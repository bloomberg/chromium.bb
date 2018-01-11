// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;
import android.net.Uri;
import android.os.Build;
import android.os.StrictMode;
import android.os.SystemClock;
import android.support.annotation.Nullable;
import android.support.v4.text.BidiFormatter;
import android.text.Editable;
import android.text.Layout;
import android.text.Selection;
import android.text.TextUtils;
import android.text.style.ReplacementSpan;
import android.util.AttributeSet;
import android.util.Pair;
import android.view.GestureDetector;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Log;
import org.chromium.base.SysUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.WindowDelegate;
import org.chromium.chrome.browser.metrics.StartupMetrics;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.UrlUtilities;
import org.chromium.ui.UiUtils;

import java.net.MalformedURLException;
import java.net.URI;
import java.net.URISyntaxException;
import java.net.URL;

/**
 * The URL text entry view for the Omnibox.
 */
public class UrlBar extends AutocompleteEditText {
    private static final String TAG = "cr_UrlBar";

    private static final boolean DEBUG = false;

    // TODO(tedchoc): Replace with EditorInfoCompat#IME_FLAG_NO_PERSONALIZED_LEARNING or
    //                EditorInfo#IME_FLAG_NO_PERSONALIZED_LEARNING as soon as either is available in
    //                all build config types.
    private static final int IME_FLAG_NO_PERSONALIZED_LEARNING = 0x1000000;

    // TextView becomes very slow on long strings, so we limit maximum length
    // of what is displayed to the user, see limitDisplayableLength().
    private static final int MAX_DISPLAYABLE_LENGTH = 4000;
    private static final int MAX_DISPLAYABLE_LENGTH_LOW_END = 1000;

    /** The contents of the URL that precede the path/query after being formatted. */
    private String mFormattedUrlLocation;

    /** The contents of the URL that precede the path/query before formatting. */
    private String mOriginalUrlLocation;

    private boolean mFirstDrawComplete;

    /**
     * The text direction of the URL or query: LAYOUT_DIRECTION_LOCALE, LAYOUT_DIRECTION_LTR, or
     * LAYOUT_DIRECTION_RTL.
     * */
    private int mUrlDirection;

    private UrlBarDelegate mUrlBarDelegate;

    private UrlDirectionListener mUrlDirectionListener;

    /**
     * The gesture detector is used to detect long presses. Long presses require special treatment
     * because the URL bar has custom touch event handling. See: {@link #onTouchEvent}.
     */
    private final GestureDetector mGestureDetector;

    private final KeyboardHideHelper mKeyboardHideHelper;

    private boolean mFocused;
    private boolean mSuppressingTouchMoveEventsForThisTouch;
    private MotionEvent mSuppressedTouchDownEvent;
    private boolean mAllowFocus = true;

    private boolean mPendingScrollTLD;
    private int mPreviousWidth;
    private String mPreviousTldScrollText;
    private int mPreviousTldScrollViewWidth;
    private int mPreviousTldScrollResultXPosition;

    private final int mDarkHintColor;
    private final int mDarkDefaultTextColor;
    private final int mDarkHighlightColor;

    private final int mLightHintColor;
    private final int mLightDefaultTextColor;
    private final int mLightHighlightColor;

    private Boolean mUseDarkColors;

    private long mFirstFocusTimeMs;

    // Used as a hint to indicate the text may contain an ellipsize span.  This will be true if an
    // ellispize span was applied the last time the text changed.  A true value here does not
    // guarantee that the text does contain the span currently as newly set text may have cleared
    // this (and it the value will only be recalculated after the text has been changed).
    private boolean mDidEllipsizeTextHint;

    /** A cached point for getting this view's location in the window. */
    private final int[] mCachedLocation = new int[2];

    /** The location of this view on the last ACTION_DOWN event. */
    private float mDownEventViewTop;

    /**
     * Implement this to get updates when the direction of the text in the URL bar changes.
     * E.g. If the user is typing a URL, then erases it and starts typing a query in Arabic,
     * the direction will change from left-to-right to right-to-left.
     */
    interface UrlDirectionListener {
        /**
         * Called whenever the layout direction of the UrlBar changes.
         * @param layoutDirection the new direction: android.view.View.LAYOUT_DIRECTION_LTR or
         *                        android.view.View.LAYOUT_DIRECTION_RTL
         */
        public void onUrlDirectionChanged(int layoutDirection);
    }

    /**
     * Delegate used to communicate with the content side and the parent layout.
     */
    public interface UrlBarDelegate {
        /**
         * @return The current active {@link Tab}. May be null.
         */
        @Nullable
        Tab getCurrentTab();

        /**
         * @return Whether the keyboard should be allowed to learn from the user input.
         */
        boolean allowKeyboardLearning();

        /**
         * Called when the text state has changed and the autocomplete suggestions should be
         * refreshed.
         */
        void onTextChangedForAutocomplete();

        /**
         * @return Whether the light security theme should be used.
         */
        boolean shouldEmphasizeHttpsScheme();

        /**
         * Called to notify that back key has been pressed while the URL bar has focus.
         */
        void backKeyPressed();
    }

    public UrlBar(Context context, AttributeSet attrs) {
        super(context, attrs);

        Resources resources = getResources();

        mDarkDefaultTextColor =
                ApiCompatibilityUtils.getColor(resources, R.color.url_emphasis_default_text);
        mDarkHintColor = ApiCompatibilityUtils.getColor(resources,
                R.color.locationbar_dark_hint_text);
        mDarkHighlightColor = getHighlightColor();

        mLightDefaultTextColor =
                ApiCompatibilityUtils.getColor(resources, R.color.url_emphasis_light_default_text);
        mLightHintColor =
                ApiCompatibilityUtils.getColor(resources, R.color.locationbar_light_hint_text);
        mLightHighlightColor = ApiCompatibilityUtils.getColor(resources,
                R.color.locationbar_light_selection_color);

        setUseDarkTextColors(true);

        mUrlDirection = LAYOUT_DIRECTION_LOCALE;

        // The URL Bar is derived from an text edit class, and as such is focusable by
        // default. This means that if it is created before the first draw of the UI it
        // will (as the only focusable element of the UI) get focus on the first draw.
        // We react to this by greying out the tab area and bringing up the keyboard,
        // which we don't want to do at startup. Prevent this by disabling focus until
        // the first draw.
        setFocusable(false);
        setFocusableInTouchMode(false);

        mGestureDetector = new GestureDetector(
                getContext(), new GestureDetector.SimpleOnGestureListener() {
                    @Override
                    public void onLongPress(MotionEvent e) {
                        performLongClick();
                    }

                    @Override
                    public boolean onSingleTapUp(MotionEvent e) {
                        requestFocus();
                        return true;
                    }
                });
        mGestureDetector.setOnDoubleTapListener(null);
        mKeyboardHideHelper = new KeyboardHideHelper(this, new Runnable() {
            @Override
            public void run() {
                if (mUrlBarDelegate != null) mUrlBarDelegate.backKeyPressed();
            }
        });

        ApiCompatibilityUtils.disableSmartSelectionTextClassifier(this);
    }

    /**
     * Initialize the delegate that allows interaction with the Window.
     */
    public void setWindowDelegate(WindowDelegate windowDelegate) {
        mKeyboardHideHelper.setWindowDelegate(windowDelegate);
    }

    /**
     * Specifies whether the URL bar should use dark text colors or light colors.
     * @param useDarkColors Whether the text colors should be dark (i.e. appropriate for use
     *                      on a light background).
     */
    public void setUseDarkTextColors(boolean useDarkColors) {
        if (mUseDarkColors != null && mUseDarkColors.booleanValue() == useDarkColors) return;

        mUseDarkColors = useDarkColors;
        if (mUseDarkColors) {
            setTextColor(mDarkDefaultTextColor);
            setHighlightColor(mDarkHighlightColor);
        } else {
            setTextColor(mLightDefaultTextColor);
            setHighlightColor(mLightHighlightColor);
        }

        // Note: Setting the hint text color only takes effect if there is not text in the URL bar.
        //       To get around this, set the URL to empty before setting the hint color and revert
        //       back to the previous text after.
        boolean hasNonEmptyText = false;
        Editable text = getText();
        if (!TextUtils.isEmpty(text)) {
            // Make sure the setText in this block does not affect the suggestions.
            setIgnoreTextChangesForAutocomplete(true);
            setText("");
            hasNonEmptyText = true;
        }
        if (useDarkColors) {
            setHintTextColor(mDarkHintColor);
        } else {
            setHintTextColor(mLightHintColor);
        }
        if (hasNonEmptyText) {
            setText(text);
            setIgnoreTextChangesForAutocomplete(false);
        }

        if (!hasFocus()) {
            emphasizeUrl();
        }
    }

    @Override
    public boolean onKeyPreIme(int keyCode, KeyEvent event) {
        if (KeyEvent.KEYCODE_BACK == keyCode && event.getAction() == KeyEvent.ACTION_UP) {
            mKeyboardHideHelper.monitorForKeyboardHidden();
        }
        return super.onKeyPreIme(keyCode, event);
    }

    /**
     * See {@link AutocompleteEditText#setIgnoreTextChangesForAutocomplete(boolean)}.
     * <p>
     * {@link #setDelegate(UrlBarDelegate)} must be called with a non-null instance prior to
     * enabling autocomplete.
     */
    @Override
    public void setIgnoreTextChangesForAutocomplete(boolean ignoreAutocomplete) {
        assert mUrlBarDelegate != null;
        super.setIgnoreTextChangesForAutocomplete(ignoreAutocomplete);
    }

    @Override
    protected void onFocusChanged(boolean focused, int direction, Rect previouslyFocusedRect) {
        mFocused = focused;
        super.onFocusChanged(focused, direction, previouslyFocusedRect);

        if (focused && mFirstFocusTimeMs == 0) {
            mFirstFocusTimeMs = SystemClock.elapsedRealtime();
        }

        if (focused) {
            StartupMetrics.getInstance().recordFocusedOmnibox();
            mPendingScrollTLD = false;
        }

        fixupTextDirection();
    }

    /**
     * @return The elapsed realtime timestamp in ms of the first time the url bar was focused,
     *         0 if never.
     */
    public long getFirstFocusTime() {
        return mFirstFocusTimeMs;
    }

    /**
     * Sets whether this {@link UrlBar} should be focusable.
     */
    public void setAllowFocus(boolean allowFocus) {
        mAllowFocus = allowFocus;
        if (mFirstDrawComplete) {
            setFocusable(allowFocus);
            setFocusableInTouchMode(allowFocus);
        }
    }

    /**
     * Sets the {@link UrlBar}'s text direction based on focus and contents.
     *
     * Should be called whenever focus or text contents change.
     */
    private void fixupTextDirection() {
        // When unfocused, force left-to-right rendering at the paragraph level (which is desired
        // for URLs). Right-to-left runs are still rendered RTL, but will not flip the whole URL
        // around. This is consistent with OmniboxViewViews on desktop. When focused, render text
        // normally (to allow users to make non-URL searches and to avoid showing Android's split
        // insertion point when an RTL user enters RTL text). Also render text normally when the
        // text field is empty (because then it displays an instruction that is not a URL).
        if (mFocused || length() == 0) {
            ApiCompatibilityUtils.setTextDirection(this, TEXT_DIRECTION_INHERIT);
        } else {
            ApiCompatibilityUtils.setTextDirection(this, TEXT_DIRECTION_LTR);
        }
        // Always align to the same as the paragraph direction (LTR = left, RTL = right).
        ApiCompatibilityUtils.setTextAlignment(this, TEXT_ALIGNMENT_TEXT_START);
    }

    @Override
    public void onWindowFocusChanged(boolean hasWindowFocus) {
        super.onWindowFocusChanged(hasWindowFocus);
        if (DEBUG) Log.i(TAG, "onWindowFocusChanged: " + hasWindowFocus);
        if (hasWindowFocus) {
            if (isFocused()) {
                // Without the call to post(..), the keyboard was not getting shown when the
                // window regained focus despite this being the final call in the view system
                // flow.
                post(new Runnable() {
                    @Override
                    public void run() {
                        UiUtils.showKeyboard(UrlBar.this);
                    }
                });
            }
        }
    }

    @Override
    public View focusSearch(int direction) {
        if (direction == View.FOCUS_BACKWARD && mUrlBarDelegate.getCurrentTab() != null
                && mUrlBarDelegate.getCurrentTab().getView() != null) {
            return mUrlBarDelegate.getCurrentTab().getView();
        } else {
            return super.focusSearch(direction);
        }
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        // This method contains special logic to enable long presses to be handled correctly.

        // One piece of the logic is to suppress all ACTION_DOWN events received while the UrlBar is
        // not focused, and only pass them to super.onTouchEvent() if it turns out we're about to
        // perform a long press. Long pressing will not behave properly without sending this event,
        // but if we always send it immediately, it will cause the keyboard to show immediately,
        // whereas we want to wait to show it until after the URL focus animation finishes, to avoid
        // performance issues on slow devices.

        // The other piece of the logic is to suppress ACTION_MOVE events received after an
        // ACTION_DOWN received while the UrlBar is not focused. This is because the UrlBar moves to
        // the side as it's focusing, and a finger held still on the screen would therefore be
        // interpreted as a drag selection.

        if (event.getActionMasked() == MotionEvent.ACTION_DOWN) {
            getLocationInWindow(mCachedLocation);
            mDownEventViewTop = mCachedLocation[1];
            mSuppressingTouchMoveEventsForThisTouch = !mFocused;
        }

        if (!mFocused) {
            if (event.getActionMasked() == MotionEvent.ACTION_DOWN) {
                mSuppressedTouchDownEvent = MotionEvent.obtain(event);
            }
            mGestureDetector.onTouchEvent(event);
            return true;
        }

        if (event.getActionMasked() == MotionEvent.ACTION_UP
                || event.getActionMasked() == MotionEvent.ACTION_CANCEL) {
            // Minor optimization to avoid unnecessarily holding onto a MotionEvent after the touch
            // finishes.
            mSuppressedTouchDownEvent = null;
        }

        if (mSuppressingTouchMoveEventsForThisTouch
                && event.getActionMasked() == MotionEvent.ACTION_MOVE) {
            return true;
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            return super.onTouchEvent(event);
        }
        try {
            return super.onTouchEvent(event);
        } catch (NullPointerException e) {
            // Working around a platform bug (b/25562038) that was fixed in N that can throw an
            // exception during text selection. We just swallow the exception. The outcome is that
            // the text selection handle doesn't show.
            Log.w(TAG, "Ignoring NPE in UrlBar#onTouchEvent.", e);
            return true;
        }
    }

    @Override
    public boolean performLongClick(float x, float y) {
        if (!shouldPerformLongClick()) return false;

        releaseSuppressedTouchDownEvent();
        return super.performLongClick(x, y);
    }

    @Override
    public boolean performLongClick() {
        if (!shouldPerformLongClick()) return false;

        releaseSuppressedTouchDownEvent();
        return super.performLongClick();
    }

    /**
     * @return Whether or not a long click should be performed.
     */
    private boolean shouldPerformLongClick() {
        getLocationInWindow(mCachedLocation);

        // If the view moved between the last down event, block the long-press.
        return mDownEventViewTop == mCachedLocation[1];
    }

    private void releaseSuppressedTouchDownEvent() {
        if (mSuppressedTouchDownEvent != null) {
            super.onTouchEvent(mSuppressedTouchDownEvent);
            mSuppressedTouchDownEvent = null;
        }
    }

    @Override
    public void onDraw(Canvas canvas) {
        super.onDraw(canvas);

        if (!mFirstDrawComplete) {
            mFirstDrawComplete = true;

            // We have now avoided the first draw problem (see the comment in
            // the constructor) so we want to make the URL bar focusable so that
            // touches etc. activate it.
            setFocusable(mAllowFocus);
            setFocusableInTouchMode(mAllowFocus);
        }

        // Notify listeners if the URL's direction has changed.
        updateUrlDirection();
    }

    /**
     * If the direction of the URL has changed, update mUrlDirection and notify the
     * UrlDirectionListeners.
     */
    private void updateUrlDirection() {
        Layout layout = getLayout();
        if (layout == null) return;

        int urlDirection;
        if (length() == 0) {
            urlDirection = LAYOUT_DIRECTION_LOCALE;
        } else if (layout.getParagraphDirection(0) == Layout.DIR_LEFT_TO_RIGHT) {
            urlDirection = LAYOUT_DIRECTION_LTR;
        } else {
            urlDirection = LAYOUT_DIRECTION_RTL;
        }

        if (urlDirection != mUrlDirection) {
            mUrlDirection = urlDirection;
            if (mUrlDirectionListener != null) {
                mUrlDirectionListener.onUrlDirectionChanged(urlDirection);
            }
        }
    }

    /**
     * @return The text direction of the URL, e.g. LAYOUT_DIRECTION_LTR.
     */
    public int getUrlDirection() {
        return mUrlDirection;
    }

    /**
     * Sets the listener for changes in the url bar's layout direction. Also calls
     * onUrlDirectionChanged() immediately on the listener.
     *
     * @param listener The UrlDirectionListener to receive callbacks when the url direction changes,
     *     or null to unregister any previously registered listener.
     */
    public void setUrlDirectionListener(UrlDirectionListener listener) {
        mUrlDirectionListener = listener;
        if (mUrlDirectionListener != null) {
            mUrlDirectionListener.onUrlDirectionChanged(mUrlDirection);
        }
    }

    /**
     * Set the url delegate to handle communication from the {@link UrlBar} to the rest of the UI.
     * @param delegate The {@link UrlBarDelegate} to be used.
     */
    public void setDelegate(UrlBarDelegate delegate) {
        mUrlBarDelegate = delegate;
    }

    @Override
    public boolean onTextContextMenuItem(int id) {
        if (id == android.R.id.paste) {
            ClipboardManager clipboard = (ClipboardManager) getContext()
                    .getSystemService(Context.CLIPBOARD_SERVICE);
            ClipData clipData = clipboard.getPrimaryClip();
            if (clipData != null) {
                StringBuilder builder = new StringBuilder();
                for (int i = 0; i < clipData.getItemCount(); i++) {
                    builder.append(clipData.getItemAt(i).coerceToText(getContext()));
                }
                String pasteString = OmniboxViewUtil.sanitizeTextForPaste(builder.toString());

                int min = 0;
                int max = getText().length();

                if (isFocused()) {
                    final int selStart = getSelectionStart();
                    final int selEnd = getSelectionEnd();

                    min = Math.max(0, Math.min(selStart, selEnd));
                    max = Math.max(0, Math.max(selStart, selEnd));
                }

                Selection.setSelection(getText(), max);
                getText().replace(min, max, pasteString);
                onPaste();
                return true;
            }
        }

        if (mOriginalUrlLocation == null || mFormattedUrlLocation == null) {
            return super.onTextContextMenuItem(id);
        }

        int selectedStartIndex = getSelectionStart();
        int selectedEndIndex = getSelectionEnd();

        // If we are copying/cutting the full previously formatted URL, reset the URL
        // text before initiating the TextViews handling of the context menu.
        String currentText = getText().toString();
        if (selectedStartIndex == 0
                && (id == android.R.id.cut || id == android.R.id.copy)
                && currentText.startsWith(mFormattedUrlLocation)
                && selectedEndIndex >= mFormattedUrlLocation.length()) {
            String newText = mOriginalUrlLocation
                    + currentText.substring(mFormattedUrlLocation.length());
            selectedEndIndex = selectedEndIndex - mFormattedUrlLocation.length()
                    + mOriginalUrlLocation.length();

            setIgnoreTextChangesForAutocomplete(true);
            setText(newText);
            setSelection(0, selectedEndIndex);
            setIgnoreTextChangesForAutocomplete(false);

            boolean retVal = super.onTextContextMenuItem(id);
            if (getText().toString().equals(newText)) {
                setIgnoreTextChangesForAutocomplete(true);
                setText(currentText);
                setSelection(getText().length());
                setIgnoreTextChangesForAutocomplete(false);
            }
            return retVal;
        }
        return super.onTextContextMenuItem(id);
    }

    /**
     * Sets the text content of the URL bar.
     *
     * @param url The original URL (or generic text) that can be used for copy/cut/paste.
     * @param formattedUrl Formatted URL for user display. Null if there isn't one.
     * @return Whether the visible text has changed.
     */
    public boolean setUrl(String url, String formattedUrl) {
        if (!TextUtils.isEmpty(formattedUrl)) {
            try {
                URL javaUrl = new URL(url);
                mFormattedUrlLocation =
                        getUrlContentsPrePath(formattedUrl, javaUrl.getHost());
                mOriginalUrlLocation =
                        getUrlContentsPrePath(url, javaUrl.getHost());
            } catch (MalformedURLException mue) {
                mOriginalUrlLocation = null;
                mFormattedUrlLocation = null;
            }
        } else {
            mOriginalUrlLocation = null;
            mFormattedUrlLocation = null;
            formattedUrl = url;
        }

        Editable previousText = getEditableText();
        setText(formattedUrl);

        boolean textChanged = !TextUtils.equals(previousText, getEditableText());
        if (textChanged && !isFocused()) scrollToTLD();
        return textChanged;
    }

    /**
     * Scroll to ensure the TLD is visible.
     */
    public void scrollToTLD() {
        if (isLayoutRequested()) {
            mPendingScrollTLD = true;
        } else {
            scrollToTLDInternal();
        }
    }

    private void scrollToTLDInternal() {
        if (mFocused) return;

        // Ensure any selection from the focus state is cleared.
        setSelection(0);

        String previousTldScrollText = mPreviousTldScrollText;
        int previousTldScrollViewWidth = mPreviousTldScrollViewWidth;
        int previousTldScrollResultXPosition = mPreviousTldScrollResultXPosition;

        mPreviousTldScrollText = null;
        mPreviousTldScrollViewWidth = 0;
        mPreviousTldScrollResultXPosition = 0;

        Editable url = getText();
        if (url == null || url.length() < 1) {
            int scrollX = 0;
            if (ApiCompatibilityUtils.isLayoutRtl(this)
                    && BidiFormatter.getInstance().isRtl(getHint())) {
                // Compared to below that uses getPrimaryHorizontal(1) due to 0 returning an
                // invalid value, if the text is empty, getPrimaryHorizontal(0) returns the actual
                // max scroll amount.
                scrollX = (int) getLayout().getPrimaryHorizontal(0) - getMeasuredWidth();
            }
            scrollTo(scrollX, getScrollY());
            return;
        }
        String urlString = url.toString();
        Pair<String, String> urlComponents =
                LocationBarLayout.splitPathFromUrlDisplayText(urlString);

        if (TextUtils.isEmpty(urlComponents.first)) {
            scrollTo(0, getScrollY());
            return;
        }

        // Do not scroll to the end of the host for URLs such as data:, javascript:, etc...
        if (urlComponents.second == null) {
            Uri uri = Uri.parse(urlString);
            String scheme = uri.getScheme();
            if (!TextUtils.isEmpty(scheme)
                    && LocationBarLayout.UNSUPPORTED_SCHEMES_TO_SPLIT.contains(scheme)) {
                scrollTo(0, getScrollY());
                return;
            }
        }

        int measuredWidth = getMeasuredWidth();
        if (TextUtils.equals(url, previousTldScrollText)
                && measuredWidth == previousTldScrollViewWidth) {
            scrollTo(previousTldScrollResultXPosition, getScrollY());
            return;
        }

        assert getLayout().getLineCount() == 1;
        float endPointX = getLayout().getPrimaryHorizontal(urlComponents.first.length());
        // Using 1 instead of 0 as zero does not return a valid value in RTL (always returns 0
        // instead of the valid scroll position).
        float startPointX = url.length() == 1 ? 0 : getLayout().getPrimaryHorizontal(1);

        float scrollPos;
        if (startPointX < endPointX) {
            // LTR
            scrollPos = Math.max(0, endPointX - measuredWidth);
        } else {
            float width = getLayout().getPaint().measureText(urlComponents.first);
            // RTL
            if (width < measuredWidth) {
                scrollPos = Math.max(0, endPointX + width - measuredWidth);
            } else {
                scrollPos = endPointX + measuredWidth;
            }
        }
        scrollTo((int) scrollPos, getScrollY());

        mPreviousTldScrollText = url.toString();
        mPreviousTldScrollViewWidth = measuredWidth;
        mPreviousTldScrollResultXPosition = (int) scrollPos;
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        super.onLayout(changed, left, top, right, bottom);

        if (mPendingScrollTLD) {
            scrollToTLDInternal();
            mPendingScrollTLD = false;
        } else if (mPreviousWidth != (right - left)) {
            scrollToTLDInternal();
            mPreviousWidth = right - left;
        }
    }

    @Override
    public boolean bringPointIntoView(int offset) {
        // TextView internally attempts to keep the selection visible, but in the unfocused state
        // this class ensures that the TLD is visible.
        if (!mFocused) return false;
        assert !mPendingScrollTLD;

        return super.bringPointIntoView(offset);
    }

    @Override
    public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
        InputConnection connection = super.onCreateInputConnection(outAttrs);
        if (mUrlBarDelegate == null || !mUrlBarDelegate.allowKeyboardLearning()) {
            outAttrs.imeOptions |= IME_FLAG_NO_PERSONALIZED_LEARNING;
        }
        return connection;
    }

    @Override
    public void setText(CharSequence text, BufferType type) {
        if (DEBUG) Log.i(TAG, "setText -- text: %s", text);
        super.setText(text, type);
        fixupTextDirection();
    }

    private void limitDisplayableLength() {
        // To limit displayable length we replace middle portion of the string with ellipsis.
        // That affects only presentation of the text, and doesn't affect other aspects like
        // copying to the clipboard, getting text with getText(), etc.
        final int maxLength = SysUtils.isLowEndDevice()
                ? MAX_DISPLAYABLE_LENGTH_LOW_END : MAX_DISPLAYABLE_LENGTH;

        Editable text = getText();
        int textLength = text.length();
        if (textLength <= maxLength) {
            if (mDidEllipsizeTextHint) {
                EllipsisSpan[] spans = text.getSpans(0, textLength, EllipsisSpan.class);
                if (spans != null && spans.length > 0) {
                    assert spans.length == 1 : "Should never apply more than a single EllipsisSpan";
                    for (int i = 0; i < spans.length; i++) {
                        text.removeSpan(spans[i]);
                    }
                }
            }
            mDidEllipsizeTextHint = false;
            return;
        }

        mDidEllipsizeTextHint = true;

        int spanLeft = text.nextSpanTransition(0, textLength, EllipsisSpan.class);
        if (spanLeft != textLength) return;

        spanLeft = maxLength / 2;
        text.setSpan(EllipsisSpan.INSTANCE, spanLeft, textLength - spanLeft,
                Editable.SPAN_INCLUSIVE_EXCLUSIVE);
    }

    /**
     * Returns the portion of the URL that precedes the path/query section of the URL.
     *
     * @param url The url to be used to find the preceding portion.
     * @param host The host to be located in the URL to determine the location of the path.
     * @return The URL contents that precede the path (or the passed in URL if the host is
     *         not found).
     */
    private static String getUrlContentsPrePath(String url, String host) {
        String urlPrePath = url;
        int hostIndex = url.indexOf(host);
        if (hostIndex >= 0) {
            int pathIndex = url.indexOf('/', hostIndex);
            if (pathIndex > 0) {
                urlPrePath = url.substring(0, pathIndex);
            } else {
                urlPrePath = url;
            }
        }
        return urlPrePath;
    }

    @Override
    public void onInitializeAccessibilityNodeInfo(AccessibilityNodeInfo info) {
        // Certain OEM implementations of onInitializeAccessibilityNodeInfo trigger disk reads
        // to access the clipboard.  crbug.com/640993
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
        try {
            super.onInitializeAccessibilityNodeInfo(info);
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }
    }

    /**
     * Emphasize components of the URL for readability.
     */
    public void emphasizeUrl() {
        Editable url = getText();
        if (hasFocus()) {
            return;
        }

        if (url.length() < 1) {
            return;
        }

        Tab currentTab = mUrlBarDelegate.getCurrentTab();
        if (currentTab == null || currentTab.getProfile() == null) return;

        boolean isInternalPage = false;
        try {
            String tabUrl = currentTab.getUrl();
            isInternalPage = UrlUtilities.isInternalScheme(new URI(tabUrl));
        } catch (URISyntaxException e) {
            // Ignore as this only is for applying color
        }

        // Since we emphasize the scheme of the URL based on the security type, we need to
        // deEmphasize first to refresh.
        deEmphasizeUrl();
        OmniboxUrlEmphasizer.emphasizeUrl(url, getResources(), currentTab.getProfile(),
                currentTab.getSecurityLevel(), isInternalPage,
                mUseDarkColors, mUrlBarDelegate.shouldEmphasizeHttpsScheme());
    }

    /**
     * Reset the modifications done to emphasize components of the URL.
     */
    public void deEmphasizeUrl() {
        OmniboxUrlEmphasizer.deEmphasizeUrl(getText());
    }

    @Override
    public CharSequence getAccessibilityClassName() {
        // When UrlBar is used as a read-only TextView, force Talkback to pronounce it like
        // TextView. Otherwise Talkback will say "Edit box, http://...". crbug.com/636988
        if (isEnabled()) {
            return super.getAccessibilityClassName();
        } else {
            return TextView.class.getName();
        }
    }

    @Override
    public void replaceAllTextFromAutocomplete(String text) {
        if (DEBUG) Log.i(TAG, "replaceAllTextFromAutocomplete: " + text);
        setUrl(text, null);
    }

    @Override
    public void onAutocompleteTextStateChanged(boolean updateDisplay) {
        if (DEBUG) {
            Log.i(TAG, "onAutocompleteTextStateChanged: DIS[%b]", updateDisplay);
        }
        if (mUrlBarDelegate == null) return;
        if (updateDisplay) limitDisplayableLength();
        // crbug.com/764749
        Log.w(TAG, "Text change observed, triggering autocomplete.");

        mUrlBarDelegate.onTextChangedForAutocomplete();
    }

    /**
     * Span that displays ellipsis instead of the text. Used to hide portion of
     * very large string to get decent performance from TextView.
     */
    private static class EllipsisSpan extends ReplacementSpan {
        private static final String ELLIPSIS = "...";

        public static final EllipsisSpan INSTANCE = new EllipsisSpan();

        @Override
        public int getSize(Paint paint, CharSequence text,
                int start, int end, Paint.FontMetricsInt fm) {
            return (int) paint.measureText(ELLIPSIS);
        }

        @Override
        public void draw(Canvas canvas, CharSequence text, int start, int end,
                float x, int top, int y, int bottom, Paint paint) {
            canvas.drawText(ELLIPSIS, x, y, paint);
        }
    }
}
