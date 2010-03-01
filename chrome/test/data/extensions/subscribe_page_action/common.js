// Whether we can modify the list of readers.
var storageEnabled = window.localStorage != null;

/**
*  Returns the default list of feed readers.
*/
function defaultReaderList() {
  // This is the default list, unless replaced by what was saved previously.
  return [
    { 'url': 'http://www.google.com/reader/view/feed/%s',
      'description': 'Google Reader'
    },
    { 'url': 'http://www.google.com/ig/adde?moduleurl=%s',
      'description': 'iGoogle'
    },
    { 'url': 'http://www.bloglines.com/login?r=/sub/%s',
      'description': 'Bloglines'
    },
    { 'url': 'http://add.my.yahoo.com/rss?url=%s',
      'description': 'My Yahoo'
    }
  ];
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
* Given the tag, find if there is a __MSG_some__ message in
* innerHTML, and replace it if there is.
*/
function substituteMessagesForTag(tag) {
  var elements = document.getElementsByTagName(tag);
  if (!elements)
    return;

  var message_format = "__MSG_([a-zA-Z0-9_@]+)__";
  for (i = 0; i < elements.length; i++) {
    var old_text = elements[i].innerHTML.match(message_format);
    if (!old_text)
      continue;

    var new_text = chrome.i18n.getMessage(old_text[1]);
    elements[i].innerHTML =
        elements[i].innerHTML.replace(old_text[0], new_text);
  }
}

/**
* Given the tag, find if the given attribute has __MSG_some__ message
* and replace it if there is.
*/
function substituteMessagesForTagAttribute(tag, attribute) {
  var elements = document.getElementsByTagName(tag);
  if (!elements)
    return;

  var message_format = "__MSG_([a-zA-Z0-9_@]+)__";
  for (i = 0; i < elements.length; i++) {
    var attribute_value = elements[i].getAttribute(attribute);
    if (!attribute_value)
      return;

    var old_text = attribute_value.match(message_format);
    if (!old_text)
      continue;

    elements[i].setAttribute(
        attribute, chrome.i18n.getMessage(old_text[1]));
  }
}