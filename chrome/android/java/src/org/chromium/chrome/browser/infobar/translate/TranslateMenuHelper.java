// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar.translate;

import android.content.Context;
import android.support.v4.content.ContextCompat;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.ListPopupWindow;
import android.widget.PopupWindow;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.infobar.TranslateOptions;

import java.util.ArrayList;
import java.util.List;

/**
 * A Helper class for managing the Translate Overflow Menu.
 */
public class TranslateMenuHelper implements AdapterView.OnItemClickListener {
    private final TranslateMenuListener mMenuListener;
    private final TranslateOptions mOptions;

    private ContextThemeWrapper mContextWrapper;
    private TranslateMenuAdapter mAdapter;
    private View mAnchorView;

    private ListPopupWindow mPopup;

    /**
     * Interface for receiving the click event of menu item.
     */
    public interface TranslateMenuListener {
        void onOverflowMenuItemClicked(int itemId);
        void onTargetMenuItemClicked(String code);
        void onSourceMenuItemClicked(String code);
    }

    public TranslateMenuHelper(Context context, View anchorView, TranslateOptions options,
            TranslateMenuListener itemListener) {
        mContextWrapper = new ContextThemeWrapper(context, R.style.OverflowMenuTheme);
        mAnchorView = anchorView;
        mOptions = options;
        mMenuListener = itemListener;
    }

    /**
     * Build transalte menu by menu type.
     */
    private List<TranslateMenu.MenuItem> getMenuList(int menuType) {
        List<TranslateMenu.MenuItem> menuList = new ArrayList<TranslateMenu.MenuItem>();
        if (menuType == TranslateMenu.MENU_OVERFLOW) {
            // TODO(googleo): Add language short list above static menu after its data is ready.
            menuList.addAll(TranslateMenu.getOverflowMenu());
        } else {
            for (int i = 0; i < mOptions.allLanguages().size(); ++i) {
                String code = mOptions.allLanguages().get(i).mLanguageCode;
                // Don't show target or source language in the menu list.
                if (code.equals(mOptions.targetLanguageCode())
                        || code.equals(mOptions.sourceLanguageCode())) {
                    continue;
                }
                menuList.add(new TranslateMenu.MenuItem(TranslateMenu.ITEM_LANGUAGE, i, code));
            }
        }
        return menuList;
    }

    /**
     * Show the overflow menu.
     */
    public void show(int menuType) {
        if (mPopup == null) {
            mPopup = new ListPopupWindow(mContextWrapper, null, android.R.attr.popupMenuStyle);
            mPopup.setModal(true);
            mPopup.setAnchorView(mAnchorView);
            mPopup.setInputMethodMode(PopupWindow.INPUT_METHOD_NOT_NEEDED);

            // Need to explicitly set the background here.  Relying on it being set in the style
            // caused an incorrectly drawn background.
            mPopup.setBackgroundDrawable(
                    ContextCompat.getDrawable(mContextWrapper, R.drawable.edge_menu_bg));

            mPopup.setOnItemClickListener(this);

            int popupWidth = mContextWrapper.getResources().getDimensionPixelSize(
                    R.dimen.infobar_translate_menu_width);
            // TODO (martiw) make the width dynamic to handle longer items.
            mPopup.setWidth(popupWidth);

            mAdapter = new TranslateMenuAdapter(menuType);
            mPopup.setAdapter(mAdapter);
        } else {
            mAdapter.refreshMenu(menuType);
        }

        if (!mPopup.isShowing()) {
            mPopup.show();
            mPopup.getListView().setItemsCanFocus(true);
        }
    }

    @Override
    public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
        dismiss();

        TranslateMenu.MenuItem item = mAdapter.getItem(position);
        switch (mAdapter.mMenuType) {
            case TranslateMenu.MENU_OVERFLOW:
                mMenuListener.onOverflowMenuItemClicked(item.mId);
                return;
            case TranslateMenu.MENU_TARGET_LANGUAGE:
                mMenuListener.onTargetMenuItemClicked(item.mCode);
                return;
            case TranslateMenu.MENU_SOURCE_LANGUAGE:
                mMenuListener.onSourceMenuItemClicked(item.mCode);
                return;
            default:
                assert false : "Unsupported Menu Item Id";
        }
    }

    /**
     * Dismisses the translate option menu.
     */
    private void dismiss() {
        if (isShowing()) {
            mPopup.dismiss();
        }
    }

    /**
     * @return Whether the app menu is currently showing.
     */
    private boolean isShowing() {
        if (mPopup == null) {
            return false;
        }
        return mPopup.isShowing();
    }

    /**
     * The provides the views of the menu items and dividers.
     */
    private final class TranslateMenuAdapter extends ArrayAdapter<TranslateMenu.MenuItem> {
        // TODO(martiw) create OVERFLOW_MENU_ITEM_WITH_CHECKBOX_CHECKED and
        // OVERFLOW_MENU_ITEM_WITH_CHECKBOX_UNCHECKED for "Always Translate Language"

        private final LayoutInflater mInflater;
        private int mMenuType;

        public TranslateMenuAdapter(int menuType) {
            super(mContextWrapper, R.layout.translate_menu_item, getMenuList(menuType));
            mInflater = LayoutInflater.from(mContextWrapper);
            mMenuType = menuType;
        }

        private void refreshMenu(int menuType) {
            // Don't refresh if the type is same as previous.
            if (menuType == mMenuType) return;

            clear();

            mMenuType = menuType;
            addAll(getMenuList(menuType));
            notifyDataSetChanged();
        }

        private String getItemViewText(TranslateMenu.MenuItem item) {
            if (mMenuType == TranslateMenu.MENU_OVERFLOW) {
                // Overflow menu items are manually defined one by one.
                String source = mOptions.sourceLanguageName();
                switch (item.mId) {
                    case TranslateMenu.ID_OVERFLOW_ALWAYS_TRANSLATE:
                        return mContextWrapper.getString(R.string.translate_always_text, source);
                    case TranslateMenu.ID_OVERFLOW_MORE_LANGUAGE:
                        return mContextWrapper.getString(R.string.translate_infobar_more_language);
                    case TranslateMenu.ID_OVERFLOW_NEVER_SITE:
                        return mContextWrapper.getString(R.string.translate_never_translate_site);
                    case TranslateMenu.ID_OVERFLOW_NEVER_LANGUAGE:
                        return mContextWrapper.getString(
                                R.string.translate_never_translate_language, source);
                    case TranslateMenu.ID_OVERFLOW_NOT_THIS_LANGUAGE:
                        return mContextWrapper.getString(
                                R.string.translate_infobar_not_source_language, source);
                    default:
                        assert false : "Unexpected Overflow Item Id";
                }
            } else {
                // Get source and tagert language menu items text by language code.
                return mOptions.getRepresentationFromCode(item.mCode);
            }
            return "";
        }

        @Override
        public int getItemViewType(int position) {
            return getItem(position).mType;
        }

        @Override
        public int getViewTypeCount() {
            return TranslateMenu.MENU_ITEM_TYPE_COUNT;
        }

        @Override
        public boolean isEnabled(int position) {
            return getItem(position).mId != TranslateMenu.ID_UNDEFINED;
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            View menuItemView = convertView;
            switch (getItemViewType(position)) {
                case TranslateMenu.ITEM_DIVIDER:
                    if (menuItemView == null) {
                        menuItemView =
                                mInflater.inflate(R.layout.translate_menu_divider, parent, false);
                    }
                    break;
                case TranslateMenu.ITEM_CHECKBOX_OPTION:
                case TranslateMenu.ITEM_TEXT_OPTION:
                // TODO(martiw) create the layout for ITEM_TEXT_OPTION and ITEM_CHECKBOX_OPTION
                case TranslateMenu.ITEM_LANGUAGE:
                    if (menuItemView == null) {
                        menuItemView =
                                mInflater.inflate(R.layout.translate_menu_item, parent, false);
                    }
                    ((TextView) menuItemView.findViewById(R.id.menu_item_text))
                            .setText(getItemViewText(getItem(position)));
                    break;
                default:
                    assert false : "Unexpected MenuItem type";
            }
            return menuItemView;
        }
    }
}
