// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import android.annotation.SuppressLint;
import android.text.TextUtils;

import org.chromium.base.annotations.CalledByNative;

import javax.annotation.Nullable;

/**
 * Provides a context in which to search, and links to the native ContextualSearchContext.
 * Includes the selection, selection offsets, surrounding page content, etc.
 * Requires an override of #onSelectionChanged to call when a non-empty selection is established
 * or changed.
 */
public abstract class ContextualSearchContext {
    static final int INVALID_OFFSET = -1;

    // Non-visible word-break marker.
    private static final int SOFT_HYPHEN_CHAR = '\u00AD';

    // Pointer to the native instance of this class.
    private long mNativePointer;

    // Whether this context has had the required properties set so it can Resolve a Search Term.
    private boolean mHasSetResolveProperties;

    // A shortened version of the actual text content surrounding the selection, or null if not yet
    // established.
    private String mSurroundingText;

    // The start and end offsets of the selection within the text content.
    private int mSelectionStartOffset = INVALID_OFFSET;
    private int mSelectionEndOffset = INVALID_OFFSET;

    // The offset of an initial Tap gesture within the text content.
    private int mTapOffset = INVALID_OFFSET;

    // The initial word selected by a Tap, or null.
    private String mInitialSelectedWord;

    // The original encoding of the base page.
    private String mEncoding;

    // The tapped word, as analyzed internally before selection takes place, or {@code null} if no
    // analysis has been done yet.
    private String mTappedWord;

    // The offset of the tap within the tapped word or {@code INVALID_OFFSET} if not yet analyzed.
    private int mTappedWordOffset = INVALID_OFFSET;

    /**
     * Constructs a context that tracks the selection and some amount of page content.
     */
    ContextualSearchContext() {
        mNativePointer = nativeInit();
        mHasSetResolveProperties = false;
    }

    /**
     * Updates a context to be able to resolve a search term and have a large amount of
     * page content.
     * @param homeCountry The country where the user usually resides, or an empty string if not
     *        known.
     * @param maySendBasePageUrl Whether policy allows sending the base-page URL to the server.
     */
    void setResolveProperties(String homeCountry, boolean maySendBasePageUrl) {
        mHasSetResolveProperties = true;
        nativeSetResolveProperties(getNativePointer(), homeCountry, maySendBasePageUrl);
    }

    /**
     * This method should be called to clean up storage when an instance of this class is
     * no longer in use.  The nativeDestroy will call the destructor on the native instance.
     */
    void destroy() {
        assert mNativePointer != 0;
        nativeDestroy(mNativePointer);
        mNativePointer = 0;

        // Also zero out private data that may be sizable.
        mSurroundingText = null;
    }

    /**
     * Sets the surrounding text and selection offsets.
     * @param encoding The original encoding of the base page.
     * @param surroundingText The text from the base page surrounding the selection.
     * @param startOffset The offset of start the selection.
     * @param endOffset The offset of the end of the selection
     */
    void setSurroundingText(
            String encoding, String surroundingText, int startOffset, int endOffset) {
        mEncoding = encoding;
        mSurroundingText = surroundingText;
        mSelectionStartOffset = startOffset;
        mSelectionEndOffset = endOffset;
        if (startOffset == endOffset && !hasAnalyzedTap()) {
            analyzeTap(startOffset);
        }
        // Notify of an initial selection if it's not empty.
        if (endOffset > startOffset) onSelectionChanged();
    }

    /**
     * @return The text that surrounds the selection, or {@code null} if none yet known.
     */
    @Nullable
    String getSurroundingText() {
        return mSurroundingText;
    }

    /**
     * @return The offset into the surrounding text of the start of the selection, or
     *         {@link #INVALID_OFFSET} if not yet established.
     */
    int getSelectionStartOffset() {
        return mSelectionStartOffset;
    }

    /**
     * @return The offset into the surrounding text of the end of the selection, or
     *         {@link #INVALID_OFFSET} if not yet established.
     */
    int getSelectionEndOffset() {
        return mSelectionEndOffset;
    }

    /**
     * @return The original encoding of the base page.
     */
    String getEncoding() {
        return mEncoding;
    }

    /**
     * @return The initial word selected by a Tap.
     */
    String getInitialSelectedWord() {
        return mInitialSelectedWord;
    }

    /**
     * @return The text content that follows the selection (one side of the surrounding text).
     */
    String getTextContentFollowingSelection() {
        if (mSurroundingText != null && mSelectionEndOffset > 0
                && mSelectionEndOffset <= mSurroundingText.length()) {
            return mSurroundingText.substring(mSelectionEndOffset);
        } else {
            return "";
        }
    }

    /**
     * @return Whether this context can Resolve the Search Term.
     */
    boolean canResolve() {
        return mHasSetResolveProperties && hasValidSelection();
    }

    /**
     * Notifies of an adjustment that has been applied to the start and end of the selection.
     * @param startAdjust A signed value indicating the direction of the adjustment to the start of
     *        the selection (typically a negative value when the selection expands).
     * @param endAdjust A signed value indicating the direction of the adjustment to the end of
     *        the selection (typically a positive value when the selection expands).
     */
    void onSelectionAdjusted(int startAdjust, int endAdjust) {
        // Fully track the selection as it changes.
        mSelectionStartOffset += startAdjust;
        mSelectionEndOffset += endAdjust;
        if (TextUtils.isEmpty(mInitialSelectedWord) && !TextUtils.isEmpty(mSurroundingText)) {
            // TODO(donnd): investigate the root cause of crbug.com/725027 that requires this
            // additional validation to prevent this substring call from crashing!
            if (mSelectionEndOffset < mSelectionStartOffset
                    || mSelectionEndOffset > mSurroundingText.length()) {
                return;
            }

            mInitialSelectedWord =
                    mSurroundingText.substring(mSelectionStartOffset, mSelectionEndOffset);
        }
        nativeAdjustSelection(getNativePointer(), startAdjust, endAdjust);
        // Notify of changes.
        onSelectionChanged();
    }

    /**
     * Notifies this instance that the selection has been changed.
     */
    abstract void onSelectionChanged();

    // ============================================================================================
    // Content Analysis.
    // ============================================================================================

    /**
     * @return Whether this context has valid Surrounding text and initial Tap offset.
     */
    private boolean hasValidTappedText() {
        return !TextUtils.isEmpty(mSurroundingText) && mTapOffset >= 0
                && mTapOffset <= mSurroundingText.length();
    }

    /**
     * @return Whether this context has a valid selection.
     */
    private boolean hasValidSelection() {
        if (!hasValidTappedText()) return false;

        return mSelectionStartOffset != INVALID_OFFSET && mSelectionEndOffset != INVALID_OFFSET
                && mSelectionStartOffset < mSelectionEndOffset
                && mSelectionEndOffset < mSurroundingText.length();
    }

    /**
     * @return Whether a Tap gesture has occurred and been analyzed.
     */
    private boolean hasAnalyzedTap() {
        return mTapOffset >= 0;
    }

    /**
     * @return The tapped word, or {@code null} if the tapped word cannot be identified by the
     *         current limited parsing capability.
     * @see #analyzeTap
     */
    String getTappedWord() {
        return mTappedWord;
    }

    /**
     * @return The offset of the tap within the tapped word, or {@code -1} if the tapped word cannot
     *         be identified by the current parsing capability.
     * @see #analyzeTap
     */
    int getTappedWordOffset() {
        return mTappedWordOffset;
    }

    /**
     * Finds the tapped word by expanding from the initial Tap offset looking for word-breaks.
     * This mimics the Blink word-segmentation invoked by SelectWordAroundCaret and similar
     * selection logic, but is only appropriate for limited use.  Does not work on ideographic
     * languages and possibly many other cases.  Should only be used only for ML signal evaluation.
     * @param tapOffset The offset of the Tap within the surrounding text.
     */
    private void analyzeTap(int tapOffset) {
        mTapOffset = tapOffset;
        mTappedWord = null;
        mTappedWordOffset = INVALID_OFFSET;

        assert hasValidTappedText();

        int wordStartOffset = findWordStartOffset(mSurroundingText, mTapOffset);
        int wordEndOffset = findWordEndOffset(mSurroundingText, mTapOffset);
        if (wordStartOffset == INVALID_OFFSET || wordEndOffset == INVALID_OFFSET) return;

        mTappedWord = mSurroundingText.substring(wordStartOffset, wordEndOffset);
        mTappedWordOffset = mTapOffset - wordStartOffset;
    }

    /**
     * Finds the offset of the start of the word that includes the given initial offset.
     * The character at the initial offset is not examined, but the one before it is, and scanning
     * continues on to earlier characters until a non-word character is found.  The offset just
     * before the non-word character is returned.  If the initial offset is a space immediately
     * following a word then the start offset of that word is returned.
     * @param text The text to scan.
     * @param initial The initial offset to scan before.
     * @return The start of the word that contains the given initial offset, within {@code text}.
     */
    private int findWordStartOffset(String text, int initial) {
        // Scan before, aborting if we hit any ideographic letter.
        for (int offset = initial - 1; offset >= 0; offset--) {
            if (isIdeographicAtIndex(text, offset)) return INVALID_OFFSET;

            if (isWordBreakAtIndex(text, offset)) {
                // The start of the word is after this word break.
                return offset + 1;
            }
        }
        return INVALID_OFFSET;
    }

    /**
     * Finds the offset of the end of the word that includes the given initial offset.
     * NOTE: this is the index of the character just past the last character of the word,
     * so a 3 character word "who" has start index 0 and end index 3.
     * The character at the initial offset is examined and each one after that too until a non-word
     * character is encountered, and that offset will be returned.
     * @param text The text to scan.
     * @param initial The initial offset to scan from.
     * @return The end of the word that contains the given initial offset, within {@code text}.
     */
    private int findWordEndOffset(String text, int initial) {
        // Scan after, aborting if we hit any CJKN letter.
        for (int offset = initial; offset < text.length(); offset++) {
            if (isIdeographicAtIndex(text, offset)) return INVALID_OFFSET;

            if (isWordBreakAtIndex(text, offset)) {
                // The end of the word is the offset of this word break.
                return offset;
            }
        }
        return INVALID_OFFSET;
    }

    /**
     * @return Whether the character at the given index in the text is "Ideographic" (as in CJKV
     *         languages), which means there may not be reliable word breaks.
     */
    @SuppressLint("NewApi")
    private boolean isIdeographicAtIndex(String text, int index) {
        return Character.isIdeographic(text.charAt(index));
    }

    /**
     * @return Whether the character at the given index is a word-break.
     */
    private boolean isWordBreakAtIndex(String text, int index) {
        return !Character.isLetterOrDigit(text.charAt(index))
                && text.codePointAt(index) != SOFT_HYPHEN_CHAR;
    }

    // ============================================================================================
    // Native callback support.
    // ============================================================================================

    @CalledByNative
    private long getNativePointer() {
        assert mNativePointer != 0;
        return mNativePointer;
    }

    // ============================================================================================
    // Native methods.
    // ============================================================================================
    private native long nativeInit();
    private native void nativeDestroy(long nativeContextualSearchContext);
    private native void nativeSetResolveProperties(
            long nativeContextualSearchContext, String homeCountry, boolean maySendBasePageUrl);
    private native void nativeAdjustSelection(
            long nativeContextualSearchContext, int startAdjust, int endAdjust);
}
