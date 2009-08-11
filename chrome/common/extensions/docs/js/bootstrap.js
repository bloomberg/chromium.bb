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
  }
}
