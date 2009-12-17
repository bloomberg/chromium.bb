/* Copyright (c) 2009 The Chromium Authors. All rights reserved.
   Use of this source code is governed by a BSD-style license that can be
   found in the LICENSE file.
*/

/* Use only multi-line comments in this file, since during testing
   its contents will get read from disk and stuffed into the
   iframe .src tag, which is a process that doesn't preserve line
   breaks and makes single-line comment out the rest of the code.
*/

/* The maximum number of feed items to show in the preview. */
var maxFeedItems = 10;

/* The maximum number of characters to show in the feed item title. */
var maxTitleCount = 64;

window.addEventListener("message", function(e) {
  var parser = new DOMParser();
  var doc = parser.parseFromString(e.data, "text/xml");

  if (doc) {
    buildPreview(doc);
  } else {
    /* Already handled in subscribe.html */
  }
}, false);

function buildPreview(doc) {
  /* Start building the part we render inside an IFRAME. We use a table to
     ensure that items are separated vertically from each other. */
  var table = document.createElement("table");
  var tbody = document.createElement("tbody");
  table.appendChild(tbody);

  /* Now parse the rest. Some use <entry> for each feed item, others use
     <channel><item>. */
  var entries = doc.getElementsByTagName('entry');
  if (entries.length == 0)
    entries = doc.getElementsByTagName('item');

  for (i = 0; i < entries.length && i < maxFeedItems; ++i) {
    item = entries.item(i);

    /* Grab the title for the feed item. */
    var itemTitle = item.getElementsByTagName('title')[0];
    if (itemTitle)
      itemTitle = itemTitle.textContent;
    else
      itemTitle = "Unknown title";

    /* Ensure max length for title. */
    if (itemTitle.length > maxTitleCount)
      itemTitle = itemTitle.substring(0, maxTitleCount) + "...";

    /* Grab the description.
       TODO(aa): Do we need to check for type=html here? */
    var itemDesc = item.getElementsByTagName('description')[0];
    if (!itemDesc)
      itemDesc = item.getElementsByTagName('summary')[0];
    if (!itemDesc)
      itemDesc = item.getElementsByTagName('content')[0];

    if (itemDesc)
      itemDesc = itemDesc.textContent;
    else
      itemDesc = "";

    /* Grab the link URL. */
    var itemLink = item.getElementsByTagName('link');
    var link = itemLink[0].childNodes[0];
    if (link)
      link = itemLink[0].childNodes[0].nodeValue;
    else
      link = itemLink[0].getAttribute('href');

    var tr = document.createElement("tr");
    var td = document.createElement("td");

    var anchor = document.createElement("a");
    anchor.id = "anchor_" + String(i);
    anchor.href = link;
    anchor.appendChild(document.createTextNode(itemTitle));
    anchor.className = "item_title";

    var span = document.createElement("span");
    span.id = "desc_" + String(i);
    span.className = "item_desc";
    span.innerHTML = itemDesc;

    td.appendChild(anchor);
    td.appendChild(document.createElement("br"));
    td.appendChild(span);
    td.appendChild(document.createElement("br"));
    td.appendChild(document.createElement("br"));

    tr.appendChild(td);
    tbody.appendChild(tr);
  }

  table.appendChild(tbody);
  document.body.appendChild(table);
}
