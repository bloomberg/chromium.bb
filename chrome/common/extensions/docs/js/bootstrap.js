// Hide body content initially to minimize flashing.
document.write('<style id="hider" type="text/css">');
document.write('body { display:none!important; }');
document.write('</style>');

window.onload = function() {
  // Regenerate page if we are passed the "?regenerate" search param
  // or if the user-agent is chrome AND the document is being served
  // from the file:/// scheme.
  if (window.location.search == "?regenerate" ||
      navigator.userAgent.indexOf("Chrome") > -1) {
    window.renderPage();
  } else {
    postRender();
  }
}

function postRender() {
  var elm = document.getElementById("hider");
  elm.parentNode.removeChild(elm);

  // Since populating the page is done asynchronously, the DOM doesn't exist
  // when the browser tries to resolve any #anchors in the URL. So we reset the
  // URL once we're done, which forces the browser to scroll to the anchor as it
  // normally would.
  if (location.hash.length > 1)
    location.href = location.href;
}
