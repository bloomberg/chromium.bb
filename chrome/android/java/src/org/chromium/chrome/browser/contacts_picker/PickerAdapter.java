// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contacts_picker;

import android.content.ContentResolver;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.net.Uri;
import android.provider.ContactsContract;
import android.support.v7.widget.RecyclerView.Adapter;
import android.support.v7.widget.RecyclerView.ViewHolder;
import android.view.LayoutInflater;
import android.view.ViewGroup;

import org.chromium.chrome.R;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Locale;
import java.util.Map;

/**
 * A data adapter for the Contacts Picker.
 */
public class PickerAdapter extends Adapter<ViewHolder> {
    // The category view to use to show the contacts.
    private PickerCategoryView mCategoryView;

    // The content resolver to query data from.
    private ContentResolver mContentResolver;

    // The full list of all registered contacts on the device.
    private ArrayList<ContactDetails> mContactDetails;

    // A list of search result indices into the larger data set.
    private ArrayList<Integer> mSearchResults;

    /**
     * Holds on to a {@link ContactView} that displays information about a contact.
     */
    public class ContactViewHolder extends ViewHolder {
        /**
         * The ContactViewHolder.
         * @param itemView The {@link ContactView} view for showing the contact details.
         */
        public ContactViewHolder(ContactView itemView) {
            super(itemView);
        }
    }

    private static final String[] PROJECTION = {
            ContactsContract.Contacts._ID, ContactsContract.Contacts.LOOKUP_KEY,
            ContactsContract.Contacts.DISPLAY_NAME_PRIMARY,
    };

    /**
     * The PickerAdapter constructor.
     * @param categoryView The category view to use to show the contacts.
     * @param contentResolver The content resolver to use to fetch the data.
     */
    public PickerAdapter(PickerCategoryView categoryView, ContentResolver contentResolver) {
        mCategoryView = categoryView;
        mContentResolver = contentResolver;
        mContactDetails = getAllContacts();
    }

    /**
     * Sets the search query (filter) for the contact list. Filtering is by display name.
     * @param query The search term to use.
     */
    public void setSearchString(String query) {
        if (query.equals("")) {
            mSearchResults.clear();
            mSearchResults = null;
        } else {
            mSearchResults = new ArrayList<Integer>();
            Integer count = 0;
            String query_lower = query.toLowerCase(Locale.getDefault());
            for (ContactDetails contact : mContactDetails) {
                if (contact.getDisplayName().toLowerCase(Locale.getDefault()).contains(query_lower)
                        || contact.getContactDetailsAsString()
                                   .toLowerCase(Locale.getDefault())
                                   .contains(query_lower)) {
                    mSearchResults.add(count);
                }
                count++;
            }
        }
        notifyDataSetChanged();
    }

    /**
     * Fetches all known contacts.
     * @return The contact list as an array.
     */
    public ArrayList<ContactDetails> getAllContacts() {
        if (mContactDetails != null) return mContactDetails;

        Map<String, ArrayList<String>> emailMap =
                getDetails(ContactsContract.CommonDataKinds.Email.CONTENT_URI,
                        ContactsContract.CommonDataKinds.Email.CONTACT_ID,
                        ContactsContract.CommonDataKinds.Email.DATA,
                        ContactsContract.CommonDataKinds.Email.CONTACT_ID + " ASC, "
                                + ContactsContract.CommonDataKinds.Email.DATA + " ASC");

        Map<String, ArrayList<String>> phoneMap =
                getDetails(ContactsContract.CommonDataKinds.Phone.CONTENT_URI,
                        ContactsContract.CommonDataKinds.Phone.CONTACT_ID,
                        ContactsContract.CommonDataKinds.Email.DATA,
                        ContactsContract.CommonDataKinds.Email.CONTACT_ID + " ASC, "
                                + ContactsContract.CommonDataKinds.Phone.NUMBER + " ASC");

        // A cursor containing the raw contacts data.
        Cursor cursor = mContentResolver.query(ContactsContract.Contacts.CONTENT_URI, PROJECTION,
                null, null, ContactsContract.Contacts.DISPLAY_NAME_PRIMARY + " ASC");

        ArrayList<ContactDetails> contacts = new ArrayList<ContactDetails>(cursor.getCount());
        if (!cursor.moveToFirst()) return contacts;
        do {
            String id = cursor.getString(cursor.getColumnIndex(ContactsContract.Contacts._ID));
            String name = cursor.getString(
                    cursor.getColumnIndex(ContactsContract.Contacts.DISPLAY_NAME_PRIMARY));
            contacts.add(new ContactDetails(id, name, emailMap.get(id), phoneMap.get(id), null));
        } while (cursor.moveToNext());

        cursor.close();
        return contacts;
    }

    // RecyclerView.Adapter:

    @Override
    public ViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
        ContactView itemView = (ContactView) LayoutInflater.from(parent.getContext())
                                       .inflate(R.layout.contact_view, parent, false);
        itemView.setCategoryView(mCategoryView);
        return new ContactViewHolder(itemView);
    }

    @Override
    public void onBindViewHolder(ViewHolder holder, int position) {
        ContactDetails contact;
        if (mSearchResults == null) {
            contact = mContactDetails.get(position);
        } else {
            Integer index = mSearchResults.get(position);
            contact = mContactDetails.get(index);
        }
        ((ContactView) holder.itemView).initialize(contact);
    }

    /**
     * Fetches details for a contact.
     * @param source The source URI to use for the lookup.
     * @param idColumn The name of the id column.
     * @param idColumn The name of the data column.
     * @param sortOrder The sort order. Data must be sorted by CONTACT_ID but can be additionally
     *                  sorted also.
     * @return A map of ids to contact details (as ArrayList).
     */
    private Map<String, ArrayList<String>> getDetails(
            Uri source, String idColumn, String dataColumn, String sortOrder) {
        Map<String, ArrayList<String>> map = new HashMap<String, ArrayList<String>>();

        Cursor cursor = mContentResolver.query(source, null, null, null, sortOrder);
        ArrayList<String> list = new ArrayList<String>();
        String key = "";
        String value;
        while (cursor.moveToNext()) {
            String id = cursor.getString(cursor.getColumnIndex(idColumn));
            value = cursor.getString(cursor.getColumnIndex(dataColumn));
            if (key.isEmpty()) {
                key = id;
                list.add(value);
            } else {
                if (key.equals(id)) {
                    list.add(value);
                } else {
                    map.put(key, list);
                    list = new ArrayList<String>();
                    list.add(value);
                    key = id;
                }
            }
        }
        map.put(key, list);
        cursor.close();

        return map;
    }

    private Bitmap getPhoto() {
        return null;
    }

    @Override
    public int getItemCount() {
        if (mSearchResults != null) return mSearchResults.size();

        return mContactDetails.size();
    }
}
