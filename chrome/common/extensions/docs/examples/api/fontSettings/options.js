// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The scripts supported by the Font Settings Extension API.
var scripts = [
  { scriptCode: 'Zyyy', scriptName: 'Default'},
  { scriptCode: 'Afak', scriptName: 'Afaka'},
  { scriptCode: 'Arab', scriptName: 'Arabic'},
  { scriptCode: 'Armi', scriptName: 'Imperial Aramaic'},
  { scriptCode: 'Armn', scriptName: 'Armenian'},
  { scriptCode: 'Avst', scriptName: 'Avestan'},
  { scriptCode: 'Bali', scriptName: 'Balinese'},
  { scriptCode: 'Bamu', scriptName: 'Bamum'},
  { scriptCode: 'Bass', scriptName: 'Bassa Vah'},
  { scriptCode: 'Batk', scriptName: 'Batak'},
  { scriptCode: 'Beng', scriptName: 'Bengali'},
  { scriptCode: 'Blis', scriptName: 'Blissymbols'},
  { scriptCode: 'Bopo', scriptName: 'Bopomofo'},
  { scriptCode: 'Brah', scriptName: 'Brahmi'},
  { scriptCode: 'Brai', scriptName: 'Braille'},
  { scriptCode: 'Bugi', scriptName: 'Buginese'},
  { scriptCode: 'Buhd', scriptName: 'Buhid'},
  { scriptCode: 'Cakm', scriptName: 'Chakma'},
  { scriptCode: 'Cans', scriptName: 'Unified Canadian Aboriginal Syllabics'},
  { scriptCode: 'Cari', scriptName: 'Carian'},
  { scriptCode: 'Cham', scriptName: 'Cham'},
  { scriptCode: 'Cher', scriptName: 'Cherokee'},
  { scriptCode: 'Cirt', scriptName: 'Cirth'},
  { scriptCode: 'Copt', scriptName: 'Coptic'},
  { scriptCode: 'Cprt', scriptName: 'Cypriot'},
  { scriptCode: 'Cyrl', scriptName: 'Cyrillic'},
  { scriptCode: 'Cyrs', scriptName: 'Old Church Slavonic Cyrillic'},
  { scriptCode: 'Deva', scriptName: 'Devanagari'},
  { scriptCode: 'Dsrt', scriptName: 'Deseret'},
  { scriptCode: 'Dupl', scriptName: 'Duployan shorthand'},
  { scriptCode: 'Egyd', scriptName: 'Egyptian demotic'},
  { scriptCode: 'Egyh', scriptName: 'Egyptian hieratic'},
  { scriptCode: 'Egyp', scriptName: 'Egyptian hieroglyphs'},
  { scriptCode: 'Elba', scriptName: 'Elbasan'},
  { scriptCode: 'Ethi', scriptName: 'Ethiopic'},
  { scriptCode: 'Geok', scriptName: 'Georgian Khutsuri'},
  { scriptCode: 'Geor', scriptName: 'Georgian'},
  { scriptCode: 'Glag', scriptName: 'Glagolitic'},
  { scriptCode: 'Goth', scriptName: 'Gothic'},
  { scriptCode: 'Gran', scriptName: 'Grantha'},
  { scriptCode: 'Grek', scriptName: 'Greek'},
  { scriptCode: 'Gujr', scriptName: 'Gujarati'},
  { scriptCode: 'Guru', scriptName: 'Gurmukhi'},
  { scriptCode: 'Hang', scriptName: 'Hangul'},
  { scriptCode: 'Hani', scriptName: 'Han'},
  { scriptCode: 'Hano', scriptName: 'Hanunoo'},
  { scriptCode: 'Hans', scriptName: 'Simplified Han'},
  { scriptCode: 'Hant', scriptName: 'Traditional Han'},
  { scriptCode: 'Hebr', scriptName: 'Hebrew'},
  { scriptCode: 'Hluw', scriptName: 'Anatolian Hieroglyphs'},
  { scriptCode: 'Hmng', scriptName: 'Pahawh Hmong'},
  { scriptCode: 'Hung', scriptName: 'Old Hungarian'},
  { scriptCode: 'Inds', scriptName: 'Indus'},
  { scriptCode: 'Ital', scriptName: 'Old Italic'},
  { scriptCode: 'Java', scriptName: 'Javanese'},
  { scriptCode: 'Jpan', scriptName: 'Japanese'},
  { scriptCode: 'Jurc', scriptName: 'Jurchen'},
  { scriptCode: 'Kali', scriptName: 'Kayah Li'},
  { scriptCode: 'Khar', scriptName: 'Kharoshthi'},
  { scriptCode: 'Khmr', scriptName: 'Khmer'},
  { scriptCode: 'Khoj', scriptName: 'Khojki'},
  { scriptCode: 'Knda', scriptName: 'Kannada'},
  { scriptCode: 'Kpel', scriptName: 'Kpelle'},
  { scriptCode: 'Kthi', scriptName: 'Kaithi'},
  { scriptCode: 'Lana', scriptName: 'Lanna'},
  { scriptCode: 'Laoo', scriptName: 'Lao'},
  { scriptCode: 'Latf', scriptName: 'Fraktur Latin'},
  { scriptCode: 'Latg', scriptName: 'Gaelic Latin'},
  { scriptCode: 'Latn', scriptName: 'Latin'},
  { scriptCode: 'Lepc', scriptName: 'Lepcha'},
  { scriptCode: 'Limb', scriptName: 'Limbu'},
  { scriptCode: 'Lina', scriptName: 'Linear A'},
  { scriptCode: 'Linb', scriptName: 'Linear B'},
  { scriptCode: 'Lisu', scriptName: 'Fraser'},
  { scriptCode: 'Loma', scriptName: 'Loma'},
  { scriptCode: 'Lyci', scriptName: 'Lycian'},
  { scriptCode: 'Lydi', scriptName: 'Lydian'},
  { scriptCode: 'Mand', scriptName: 'Mandaean'},
  { scriptCode: 'Mani', scriptName: 'Manichaean'},
  { scriptCode: 'Maya', scriptName: 'Mayan hieroglyphs'},
  { scriptCode: 'Mend', scriptName: 'Mende'},
  { scriptCode: 'Merc', scriptName: 'Meroitic Cursive'},
  { scriptCode: 'Mero', scriptName: 'Meroitic'},
  { scriptCode: 'Mlym', scriptName: 'Malayalam'},
  { scriptCode: 'Mong', scriptName: 'Mongolian'},
  { scriptCode: 'Moon', scriptName: 'Moon'},
  { scriptCode: 'Mroo', scriptName: 'Mro'},
  { scriptCode: 'Mtei', scriptName: 'Meitei Mayek'},
  { scriptCode: 'Mymr', scriptName: 'Myanmar'},
  { scriptCode: 'Narb', scriptName: 'Old North Arabian'},
  { scriptCode: 'Nbat', scriptName: 'Nabataean'},
  { scriptCode: 'Nkgb', scriptName: 'Naxi Geba'},
  { scriptCode: 'Nkoo', scriptName: 'N’Ko'},
  { scriptCode: 'Nshu', scriptName: 'Nüshu'},
  { scriptCode: 'Ogam', scriptName: 'Ogham'},
  { scriptCode: 'Olck', scriptName: 'Ol Chiki'},
  { scriptCode: 'Orkh', scriptName: 'Orkhon'},
  { scriptCode: 'Orya', scriptName: 'Oriya'},
  { scriptCode: 'Osma', scriptName: 'Osmanya'},
  { scriptCode: 'Palm', scriptName: 'Palmyrene'},
  { scriptCode: 'Perm', scriptName: 'Old Permic'},
  { scriptCode: 'Phag', scriptName: 'Phags-pa'},
  { scriptCode: 'Phli', scriptName: 'Inscriptional Pahlavi'},
  { scriptCode: 'Phlp', scriptName: 'Psalter Pahlavi'},
  { scriptCode: 'Phlv', scriptName: 'Book Pahlavi'},
  { scriptCode: 'Phnx', scriptName: 'Phoenician'},
  { scriptCode: 'Plrd', scriptName: 'Pollard Phonetic'},
  { scriptCode: 'Prti', scriptName: 'Inscriptional Parthian'},
  { scriptCode: 'Rjng', scriptName: 'Rejang'},
  { scriptCode: 'Roro', scriptName: 'Rongorongo'},
  { scriptCode: 'Runr', scriptName: 'Runic'},
  { scriptCode: 'Samr', scriptName: 'Samaritan'},
  { scriptCode: 'Sara', scriptName: 'Sarati'},
  { scriptCode: 'Sarb', scriptName: 'Old South Arabian'},
  { scriptCode: 'Saur', scriptName: 'Saurashtra'},
  { scriptCode: 'Sgnw', scriptName: 'SignWriting'},
  { scriptCode: 'Shaw', scriptName: 'Shavian'},
  { scriptCode: 'Shrd', scriptName: 'Sharada'},
  { scriptCode: 'Sind', scriptName: 'Khudawadi'},
  { scriptCode: 'Sinh', scriptName: 'Sinhala'},
  { scriptCode: 'Sora', scriptName: 'Sora Sompeng'},
  { scriptCode: 'Sund', scriptName: 'Sundanese'},
  { scriptCode: 'Sylo', scriptName: 'Syloti Nagri'},
  { scriptCode: 'Syrc', scriptName: 'Syriac'},
  { scriptCode: 'Syre', scriptName: 'Estrangelo Syriac'},
  { scriptCode: 'Syrj', scriptName: 'Western Syriac'},
  { scriptCode: 'Syrn', scriptName: 'Eastern Syriac'},
  { scriptCode: 'Tagb', scriptName: 'Tagbanwa'},
  { scriptCode: 'Takr', scriptName: 'Takri'},
  { scriptCode: 'Tale', scriptName: 'Tai Le'},
  { scriptCode: 'Talu', scriptName: 'New Tai Lue'},
  { scriptCode: 'Taml', scriptName: 'Tamil'},
  { scriptCode: 'Tang', scriptName: 'Tangut'},
  { scriptCode: 'Tavt', scriptName: 'Tai Viet'},
  { scriptCode: 'Telu', scriptName: 'Telugu'},
  { scriptCode: 'Teng', scriptName: 'Tengwar'},
  { scriptCode: 'Tfng', scriptName: 'Tifinagh'},
  { scriptCode: 'Tglg', scriptName: 'Tagalog'},
  { scriptCode: 'Thaa', scriptName: 'Thaana'},
  { scriptCode: 'Thai', scriptName: 'Thai'},
  { scriptCode: 'Tibt', scriptName: 'Tibetan'},
  { scriptCode: 'Tirh', scriptName: 'Tirhuta'},
  { scriptCode: 'Ugar', scriptName: 'Ugaritic'},
  { scriptCode: 'Vaii', scriptName: 'Vai'},
  { scriptCode: 'Visp', scriptName: 'Visible Speech'},
  { scriptCode: 'Wara', scriptName: 'Varang Kshiti'},
  { scriptCode: 'Wole', scriptName: 'Woleai'},
  { scriptCode: 'Xpeo', scriptName: 'Old Persian'},
  { scriptCode: 'Xsux', scriptName: 'Sumero-Akkadian Cuneiform'},
  { scriptCode: 'Yiii', scriptName: 'Yi'},
  { scriptCode: 'Zmth', scriptName: 'Mathematical Notation'},
  { scriptCode: 'Zsym', scriptName: 'Symbols'}
];

// The generic font families supported by the Font Settings Extension API.
var families =
    ["standard", "sansserif", "serif", "fixed", "cursive", "fantasy"];

// Mapping between font list ids and the generic family setting they
// represent.
var fontPickers = [
  { fontList: 'standardFontList', name: 'standard' },
  { fontList: 'serifFontList', name: 'serif' },
  { fontList: 'sansSerifFontList', name: 'sansserif' },
  { fontList: 'fixedFontList', name: 'fixed' }
];

// Ids of elements to contain the sample text.
var sampleTextDivIds = [
  'standardFontSample',
  'serifFontSample',
  'sansSerifFontSample',
  'fixedFontSample',
  'minFontSample'
];

// Sample texts.
var defaultSampleText = 'The quick brown fox jumps over the lazy dog.';
var scriptSpecificSampleText = {
  // "Cyrllic script".
  'Cyrl': 'Кириллица',
  'Hang': '정 참판 양반댁 규수 큰 교자 타고 혼례 치른 날.',
  'Hans': '床前明月光，疑是地上霜。举头望明月，低头思故乡。',
  'Hant': '床前明月光，疑是地上霜。舉頭望明月，低頭思故鄉。',
  'Jpan': '吾輩は猫である。名前はまだ無い。',
  // "Khmer language".
  'Khmr': '\u1797\u17B6\u179F\u17B6\u1781\u17D2\u1798\u17C2\u179A',
};

// Definition for ScriptList.
cr.define('fontSettings.ui', function() {
  const List = cr.ui.List;
  const ListItem = cr.ui.ListItem;
  const ListSingleSelectionModel = cr.ui.ListSingleSelectionModel;

  function ScriptListItem(info) {
    var el = cr.doc.createElement('li');
    el.__proto__ = ScriptListItem.prototype;
    el.info_ = info;
    el.decorate();
    return el;
  };

  ScriptListItem.prototype = {
    __proto__: ListItem.prototype,

    decorate: function() {
      this.textContent = this.info_.scriptName;
      if (this.info_.scriptCode == 'Zyyy') {
        this.style.marginBottom = '1em';
      }
    }
  };

  var ScriptList = cr.ui.define('list');
  ScriptList.prototype = {
    __proto__: List.prototype,

    decorate: function() {
      List.prototype.decorate.call(this);
      var sm = new ListSingleSelectionModel();
      this.selectionModel = sm;
      this.autoExpands = true;
      this.dataModel = new cr.ui.ArrayDataModel(scripts);
      this.style.height = '75vh';
    },

    createItem: function(info) {
      return new ScriptListItem(info);
    }
  };

  return {
    ScriptList: ScriptList,
    ScriptListItem: ScriptListItem
  };
});

function getSelectedScript() {
  var scriptList = document.getElementById('scriptList');
  return scriptList.selectedItem.scriptCode;
}

function getSelectedFont(fontList) {
  return fontList.options[fontList.selectedIndex].value;
}

// Populates the font lists with the list of system fonts from |fonts|.
function populateLists(fonts) {
  for (var i = 0; i < fontPickers.length; i++) {
    var list = document.getElementById(fontPickers[i].fontList);

    // Add special item to indicate fallback to the non-per-script
    // font setting. The Font Settings API uses the empty string to indicate
    // fallback.
    var defaultItem = document.createElement('option');
    defaultItem.value = '';
    defaultItem.text = '(Use default)';
    list.add(defaultItem);

    for (var j = 0; j < fonts.length; j++) {
      var item = document.createElement('option');
      item.value = fonts[j].fontId;
      item.text = fonts[j].displayName;
      list.add(item);
    }
  }

  updateFontListsForScript();
}

// Returns a function that updates the font setting for |genericFamily|
// to match the selected value in |fontList|. It can be used as an event
// handler for selection changes in |fontList|.
function getFontChangeHandler(fontList, genericFamily) {
  return function() {
    var script = getSelectedScript();
    var font = getSelectedFont(fontList);

    var details = {};
    details.genericFamily = genericFamily;
    details.fontId = font;
    details.script = script;

    chrome.fontSettings.setFont(details);
  };
}

// Sets the selected value of |fontList| to |fontId|.
function setSelectedFont(fontList, fontId) {
  var script = getSelectedScript();
  var i;
  for (i = 0; i < fontList.length; i++) {
    if (fontId == fontList.options[i].value) {
      fontList.selectedIndex = i;
      break;
    }
  }
  if (i == fontList.length) {
    console.warn("font '" + fontId + "' for " + fontList.id + ' for ' +
        script + ' is not on the system');
  }
}

// Returns a callback function that sets the selected value of |list| to the
// font returned from |chrome.fontSettings.getFont|.
function getFontHandler(list) {
  return function(details) {
    setSelectedFont(list, details.fontId);
    list.disabled = !isControllableLevel(details.levelOfControl);
  };
}

// Called when the script list selection changes. Sets the selected value of
// each font list to the current font setting, and updates the samples' lang
// so that they are shown in the current font setting.
function updateFontListsForScript() {
  var script = getSelectedScript();

  for (var i = 0; i < fontPickers.length; i++) {
    var list = document.getElementById(fontPickers[i].fontList);
    var family = fontPickers[i].name;

    var details = {};
    details.genericFamily = family;
    details.script = script;
    chrome.fontSettings.getFont(details, getFontHandler(list));
  }

  if (typeof(scriptSpecificSampleText[script]) != 'undefined')
    sample = scriptSpecificSampleText[script];
  else
    sample = defaultSampleText;
  for (var i = 0; i < sampleTextDivIds.length; i++) {
    var sampleTextDiv = document.getElementById(sampleTextDivIds[i]);
    // For font selection it's the script code that matters, not language, so
    // just use en for lang.
    sampleTextDiv.lang = 'en-' + script;
    sampleTextDiv.innerText = sample;
  }
}

// Returns a function to be called when the user changes the font size
// input element |elem|. The function calls the Font Settings Extension API
// function |setter| to commit the change.
function getFontSizeChangedFunc(elem, setter) {
  return function() {
    var pixelSize = parseInt(elem.value);
    if (!isNaN(pixelSize)) {
      setter({ pixelSize: pixelSize });
    }
  }
}

function isControllableLevel(levelOfControl) {
  return levelOfControl == 'controllable_by_this_extension' ||
      levelOfControl == 'controlled_by_this_extension';
}

// Returns a function to be used as a listener for font size setting changed
// events from the Font Settings Extension API. The function updates the input
// element |elem| and the elements in |sampleTexts| to reflect the change.
function getFontSizeChangedOnBrowserFunc(elem, sampleTexts) {
  return function(details) {
    var size = details.pixelSize.toString();
    elem.value = size;
    elem.disabled = !isControllableLevel(details.levelOfControl);
    for (var i = 0; i < sampleTexts.length; i++)
      document.getElementById(sampleTexts[i]).style.fontSize = size + 'px';
  }
}

// Maps the HTML <input> element with |id| to the extension API accessor
// functions |getter| and |setter| for a setting and onChange event |apiEvent|
// for the setting. Also, maps the element ids in |sampleTexts| to this setting.
function initFontSizePref(id, sampleTexts, getter, setter, apiEvent) {
  var elem = document.getElementById(id);
  getter({}, function(details) {
    var size = details.pixelSize.toString();
    elem.value = size;
    elem.disabled = !isControllableLevel(details.levelOfControl);
    for (var i = 0; i < sampleTexts.length; i++)
      document.getElementById(sampleTexts[i]).style.fontSize = size + 'px';
  });
  elem.addEventListener('change', getFontSizeChangedFunc(elem, setter));
  apiEvent.addListener(getFontSizeChangedOnBrowserFunc(elem, sampleTexts));
}

function clearSettingsForScript(script) {
  for (var i = 0; i < families.length; i++) {
    chrome.fontSettings.clearFont({
      script: script,
      genericFamily: families[i]
    });
  }
}

function clearAllSettings() {
  for (var i = 0; i < scripts.length; i++)
    clearSettingsForScript(scripts[i].scriptCode);

  chrome.fontSettings.clearDefaultFixedFontSize();
  chrome.fontSettings.clearDefaultFontSize();
  chrome.fontSettings.clearMinimumFontSize();
}

function closeOverlay() {
  $('overlay-container').hidden = true;
}

function initResetButtons() {
  var overlay = $('overlay-container');
  cr.ui.overlay.globalInitialization();
  cr.ui.overlay.setupOverlay(overlay);
  overlay.addEventListener('cancelOverlay', closeOverlay);

  $('reset-this-script-button').onclick = function(event) {
    var scriptName = $('scriptList').selectedItem.scriptName;
    $('reset-this-script-overlay-dialog-content').innerText =
        'Are you sure you want to reset settings for ' + scriptName +
        ' script?';

    $('overlay-container').hidden = false;
    $('reset-this-script-overlay-dialog').hidden = false;
    $('reset-all-scripts-overlay-dialog').hidden = true;
  }
  $('reset-this-script-ok').onclick = function(event) {
    clearSettingsForScript(getSelectedScript());
    closeOverlay();
  };
  $('reset-this-script-cancel').onclick = closeOverlay;

  $('reset-all-button').onclick = function(event) {
    $('overlay-container').hidden = false;
    $('reset-all-scripts-overlay-dialog').hidden = false;
    $('reset-this-script-overlay-dialog').hidden = true;
  }
  $('reset-all-ok').onclick = function(event) {
    clearAllSettings();
    closeOverlay();
  }
  $('reset-all-cancel').onclick = closeOverlay;
}

function init() {
  var scriptList = document.getElementById('scriptList');
  fontSettings.ui.ScriptList.decorate(scriptList);
  scriptList.selectionModel.selectedIndex = 0;
  scriptList.selectionModel.addEventListener('change',
                                             updateFontListsForScript);

  // Populate the font lists.
  chrome.fontSettings.getFontList(populateLists);

  // Add change handlers to the font lists.
  for (var i = 0; i < fontPickers.length; i++) {
    var list = document.getElementById(fontPickers[i].fontList);
    var handler = getFontChangeHandler(list, fontPickers[i].name);
    list.addEventListener('change', handler);
  }

  chrome.fontSettings.onFontChanged.addListener(
      updateFontListsForScript);

  initFontSizePref(
      'defaultFontSizeRocker',
      ['standardFontSample', 'serifFontSample', 'sansSerifFontSample'],
      chrome.fontSettings.getDefaultFontSize,
      chrome.fontSettings.setDefaultFontSize,
      chrome.fontSettings.onDefaultFontSizeChanged);
  initFontSizePref(
      'defaultFontSizeRange',
      ['standardFontSample', 'serifFontSample', 'sansSerifFontSample'],
      chrome.fontSettings.getDefaultFontSize,
      chrome.fontSettings.setDefaultFontSize,
      chrome.fontSettings.onDefaultFontSizeChanged);
  initFontSizePref(
      'defaultFixedFontSizeRocker',
      ['fixedFontSample'],
      chrome.fontSettings.getDefaultFixedFontSize,
      chrome.fontSettings.setDefaultFixedFontSize,
      chrome.fontSettings.onDefaultFixedFontSizeChanged);
  initFontSizePref(
      'defaultFixedFontSizeRange',
      ['fixedFontSample'],
      chrome.fontSettings.getDefaultFixedFontSize,
      chrome.fontSettings.setDefaultFixedFontSize,
      chrome.fontSettings.onDefaultFixedFontSizeChanged);
  initFontSizePref(
      'minFontSizeRocker',
      ['minFontSample'],
      chrome.fontSettings.getMinimumFontSize,
      chrome.fontSettings.setMinimumFontSize,
      chrome.fontSettings.onMinimumFontSizeChanged);
  initFontSizePref(
      'minFontSizeRange',
      ['minFontSample'],
      chrome.fontSettings.getMinimumFontSize,
      chrome.fontSettings.setMinimumFontSize,
      chrome.fontSettings.onMinimumFontSizeChanged);

  initResetButtons();
}

document.addEventListener('DOMContentLoaded', init);
