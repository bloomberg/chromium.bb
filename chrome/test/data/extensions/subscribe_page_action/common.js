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
