// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Grab the querystring, removing question mark at the front and splitting on
// the ampersand.
var queryString = location.search.substring(1).split("&");

// The feed URL is the first component and always present.
var feedUrl = decodeURIComponent(queryString[0]);

// We allow synchronous requests for testing. This component is only present
// if true.
var synchronousRequest = queryString[1] == "synchronous";

// The XMLHttpRequest object that tries to load and parse the feed, and (if
// testing) also the style sheet and the frame js.
var req;

// Depending on whether this is run from a test or from the extension, this
// will either be a link to the css file within the extension or contain the
// contents of the style sheet, fetched through XmlHttpRequest.
var styleSheet = "";

// Depending on whether this is run from a test or from the extension, this
// will either be a link to the js file within the extension or contain the
// contents of the style sheet, fetched through XmlHttpRequest.
var frameScript = "";

// What to show when we cannot parse the feed name.
var unknownName = "Unknown feed name";

// Various text messages within the edit dialog.
var assistText = "Insert %s in the URL where the feed url should appear.";
var savedMsg = "Your changes have been saved.";
var setDefaultMsg = "This reader has been set as your default reader.";

// A list of feed readers, populated by localStorage if available, otherwise
// hard coded.
var feedReaderList;

// Whether we can modify the list of readers.
var storageEnabled = window.localStorage != null;

// Navigates to the reader of the user's choice (for subscribing to the feed).
function navigate() {
  var select = document.getElementById('readerDropdown');
  var url =
      feedReaderList[select.selectedIndex].url.replace(
          "%s", escape(encodeURI(feedUrl)));
  document.location = url;
}

/**
* The main function. Sets up the selection list for possible readers and
* fetches the data.
*/
function main() {
  // This is the default list, unless replaced by what was saved previously.
  feedReaderList = [
      { 'url': 'http://www.google.com/reader/view/feed/%s',
        'description': 'Google Reader' },
      { 'url': 'http://www.google.com/ig/adde?moduleurl=%s',
        'description': 'iGoogle' },
      { 'url': 'http://www.bloglines.com/login?r=/sub/%s',
        'description': 'Bloglines' },
      { 'url': 'http://add.my.yahoo.com/rss?url=%s',
        'description': 'My Yahoo' }];

  if (!storageEnabled) {
    // No local storage means we can't make changes to the list, so disable
    // the Edit and the Remove link permanently.
    document.getElementById('editLink').innerHTML = "";
    document.getElementById('removeLink').innerHTML = "";
  } else {
    if (window.localStorage.readerList)
      feedReaderList = JSON.parse(window.localStorage.readerList);
  }

  // Populate the list of readers.
  var readerDropdown = document.getElementById('readerDropdown');
  for (i = 0; i < feedReaderList.length; ++i) {
    readerDropdown.options[i] = new Option(feedReaderList[i].description, i);
    if (storageEnabled && isDefaultReader(feedReaderList[i].url))
      readerDropdown.selectedIndex = i;
  }

  if (storageEnabled)
    readerDropdown.options[i] = new Option("Add new...", "");

  // Now fetch the data.
  req = new XMLHttpRequest();
  if (synchronousRequest) {
    // Tests that load the html page directly through a file:// url don't have
    // access to the js and css from the frame so we must load them first and
    // inject them into the src for the iframe.
    req.open("GET", "style.css", false);
    req.send(null);

    styleSheet = "<style>" + req.responseText + "</style>";

    req.open("GET", "iframe.js", false);
    req.send(null);

    frameScript = "<script>" + req.responseText +
                    "<" + "/script>";
  } else {
    // Normal loading just requires links to the css and the js file.
    styleSheet = "<link rel='stylesheet' type='text/css' href='" +
                    chrome.extension.getURL("style.css") + "'>";
    frameScript = "<script src='" + chrome.extension.getURL("iframe.js") +
                    "'></" + "script>";
  }

  feedUrl = decodeURIComponent(feedUrl);
  req.onload = handleResponse;
  req.onerror = handleError;
  req.open("GET", feedUrl, !synchronousRequest);
  req.send(null);

  document.getElementById('feedUrl').href = feedUrl;
}

// Sets the title for the feed.
function setFeedTitle(title) {
  var titleTag = document.getElementById('title');
  titleTag.textContent = "Feed for '" + title + "'";
}

// Handles errors during the XMLHttpRequest.
function handleError() {
  handleFeedParsingFailed("Error fetching feed");
}

// Handles feed parsing errors.
function handleFeedParsingFailed(error) {
  setFeedTitle(unknownName);

  // The tests always expect an IFRAME, so add one showing the error.
  var html = "<body><span id=\"error\" class=\"item_desc\">" + error +
               "</span></body>";

  var error_frame = createFrame('error', html);
  var itemsTag = document.getElementById('items');
  itemsTag.appendChild(error_frame);
}

function createFrame(frame_id, html) {
  frame = document.createElement('iframe');
  frame.id = frame_id;
  frame.src = "data:text/html;charset=utf-8,<html>" + styleSheet + html +
                "</html>";
  frame.scrolling = "auto";
  frame.frameBorder = "0";
  frame.marginWidth = "0";
  return frame;
}

function embedAsIframe(rssText) {
  var itemsTag = document.getElementById('items');

  // TODO(aa): Add base URL tag
  iframe = createFrame('rss', styleSheet + frameScript);
  itemsTag.appendChild(iframe);

  iframe.onload = function() {
    iframe.contentWindow.postMessage(rssText, "*");
  }
}

// Handles parsing the feed data we got back from XMLHttpRequest.
function handleResponse() {
  // Uncomment these three lines to see what the feed data looks like.
  // var itemsTag = document.getElementById('items');
  // itemsTag.textContent = req.responseText;
  // return;

  var doc = req.responseXML;
  if (!doc) {
    handleFeedParsingFailed("Not a valid feed.");
    return;
  }

  // We must find at least one 'entry' or 'item' element before proceeding.
  var entries = doc.getElementsByTagName('entry');
  if (entries.length == 0)
    entries = doc.getElementsByTagName('item');
  if (entries.length == 0) {
    handleFeedParsingFailed("This feed contains no entries.")
    return;
  }

  // Figure out what the title of the whole feed is.
  var title = doc.getElementsByTagName('title')[0];
  if (title)
    setFeedTitle(title.textContent);
  else
    setFeedTitle(unknownName);

  // Add an IFRAME with the html contents.
  embedAsIframe(req.responseText);
}

/**
* Check to see if the current item is set as default reader.
*/
function isDefaultReader(url) {
  defaultReader = window.localStorage.defaultReader ?
                      window.localStorage.defaultReader : "";
  return url == defaultReader;
}

/**
* Shows the Edit Feed Reader dialog.
*/
function showDialog() {
  // Set the values for this dialog box before showing it.
  document.getElementById('statusMsg').innerText = "";
  document.getElementById('urlAssist').innerText = assistText;
  document.getElementById('setDefault').display = "none";

  var readerDropdown = document.getElementById('readerDropdown');

  if (readerDropdown.value != "") {
    // We are not adding a new value, so we populate the dialog box.
    document.getElementById('urlText').value =
        feedReaderList[readerDropdown.value].url;
    document.getElementById('descriptionText').value =
        readerDropdown.options[readerDropdown.selectedIndex].text;
  }

  validateInput(null);

  var url = document.getElementById('urlText').value;
  if (url.length > 0 && !isDefaultReader(url)) {
    var setDefault = document.getElementById('setDefault');
    setDefault.disabled = false;
    setDefault.style.display = "inline";
  }

  // Show the dialog box.
  document.getElementById('dialogBackground').style.display = "-webkit-box";
}

/**
* Hides the Edit Feed Reader dialog.
*/
function hideDialog() {
  var selected = readerDropdown.selectedIndex;
  var addNew = readerDropdown.value.length == 0;

  var setDefault = document.getElementById('setDefault');
  setDefault.disabled = true;
  setDefault.style.display = "none";

  document.getElementById('dialogBackground').style.display = "none";

  if (addNew && readerDropdown[selected].text == "")
    removeEntry(true);  // User cancelled. Remove the entry without prompting.
}

function saveFeedReaderList() {
  window.localStorage.readerList = JSON.stringify(feedReaderList);
}

/**
* Handler for saving the values.
*/
function save() {
  var readerDropdown = document.getElementById('readerDropdown');
  var selected = readerDropdown.selectedIndex;
  var addNew = readerDropdown.value.length == 0;
  var oldUrl = addNew ? "" : feedReaderList[selected].url;
  var url = document.getElementById('urlText').value;
  var description = document.getElementById('descriptionText').value;

  // Update the UI to reflect the changes.
  readerDropdown.options[selected].text = description;
  readerDropdown.options[selected].value = selected;
  document.getElementById('statusMsg').innerText = savedMsg;
  document.getElementById('save').disabled = true;

  if (addNew) {
    feedReaderList.push({ 'url': url, 'description': description });
  } else {
    feedReaderList[selected].description = description;
    feedReaderList[selected].url = url;
  }

  saveFeedReaderList();

  var wasDefault = isDefaultReader(oldUrl);

  if (wasDefault) {
    // If this was the default, it should still be the default after we
    // modify it.
    window.localStorage.defaultReader = url;
  } else if (!isDefaultReader(url)) {
    // Enable the set as default button.
    var setDefault = document.getElementById('setDefault');
    setDefault.disabled = false;
    setDefault.style.display = "inline";
  }
}

/**
* Removes an entry from the list.
*/
function removeEntry(force) {
  var selected = readerDropdown.selectedIndex;

  if (!force) {
    // Minimum number of entries (1 plus "Add New...").
    var minEntries = 2;
    if (readerDropdown.options.length == minEntries) {
      alert('There must be at least one entry in this list');
      return;
    }

    var reply = confirm("Are you sure you want to remove '" +
                          readerDropdown[selected].text + "'?");
    if (!reply)
      return;
  }

  feedReaderList.splice(selected, 1);
  readerDropdown.remove(selected);
  saveFeedReaderList();
}

/**
* Validates the input in the form (making sure something is entered in both
* fields and that %s is not missing from the url field.
*/
function validateInput() {
  document.getElementById('statusMsg').innerText = "";
  document.getElementById('setDefault').disabled = true;

  var description = document.getElementById('descriptionText');
  var url = document.getElementById('urlText');

  var valid = description.value.length > 0 &&
                url.value.length > 0 &&
                url.value.indexOf("%s") > -1;

  document.getElementById('save').disabled = !valid;
}

/**
* Handler for setting the current reader as default.
*/
function setAsDefault() {
  window.localStorage.defaultReader =
        document.getElementById('urlText').value;

  document.getElementById('statusMsg').innerText = setDefaultMsg;

  document.getElementById('setDefault').disabled = true;
}

/**
* Handler for when selection changes.
*/
function onSelectChanged() {
  if (!storageEnabled)
    return;

  var readerDropdown = document.getElementById('readerDropdown');

  // If the last item (Add new...) was selected we need to add an empty item
  // for it. But adding values to the dropdown resets the selectedIndex, so we
  // need to save it first.
  var oldSelection = readerDropdown.selectedIndex;
  if (readerDropdown.selectedIndex == readerDropdown.length - 1) {
    readerDropdown[readerDropdown.selectedIndex] = new Option("", "");
    readerDropdown.options[readerDropdown.length] =
          new Option("Add new...", "");
    readerDropdown.selectedIndex = oldSelection;
    document.getElementById('urlText').value = "";
    document.getElementById('descriptionText').value = "";
    showDialog();
  }
}
