// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import android.support.annotation.CallSuper;

/**
 * A node in the tree that has a parent and can notify it about changes.
 *
 * This class mostly serves as a convenience base class for implementations of {@link TreeNode}.
 */
public abstract class ChildNode implements TreeNode {
    private NodeParent mParent;

    @Override
    @CallSuper
    public void setParent(NodeParent parent) {
        assert mParent == null;
        assert parent != null;
        mParent = parent;
    }

    @Override
    @CallSuper
    public void detach() {
        assert mParent != null;
        mParent = null;
    }

    protected void notifyItemRangeChanged(int index, int count, Object payload) {
        if (mParent != null) mParent.onItemRangeChanged(this, index, count, payload);
    }

    protected void notifyItemRangeChanged(int index, int count) {
        notifyItemRangeChanged(index, count, null);
    }

    protected void notifyItemRangeInserted(int index, int count) {
        if (mParent != null) mParent.onItemRangeInserted(this, index, count);
    }

    protected void notifyItemRangeRemoved(int index, int count) {
        if (mParent != null) mParent.onItemRangeRemoved(this, index, count);
    }

    protected void notifyItemChanged(int index, Object payload) {
        notifyItemRangeChanged(index, 1, payload);
    }

    protected void notifyItemChanged(int index) {
        notifyItemRangeChanged(index, 1);
    }

    protected void notifyItemInserted(int index) {
        notifyItemRangeInserted(index, 1);
    }

    protected void notifyItemRemoved(int index) {
        notifyItemRangeRemoved(index, 1);
    }

    protected void checkIndex(int position) {
        if (position < 0 || position >= getItemCount()) {
            throw new IndexOutOfBoundsException(position + "/" + getItemCount());
        }
    }
}
