// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Mapping between font list ids and the generic family setting they
// represent.
var genericFamilies = [
  { fontList: 'standardFontList', name: 'standard' },
  { fontList: 'serifFontList', name: 'serif' },
  { fontList: 'sansSerifFontList', name: 'sansserif' },
  { fontList: 'fixedFontList', name: 'fixed' }
];

// Ids of elements to contain the "Lorem ipsum" sample text.
var sampleTextDivIds = [
  'standardFontSample',
  'serifFontSample',
  'sansSerifFontSample',
  'fixedFontSample',
  'minFontSample'
];

var defaultSampleText = 'Lorem ipsum dolor sit amat.';
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

function getSelectedScript() {
  var scriptList = document.getElementById('scriptList');
  return scriptList.options[scriptList.selectedIndex].value;
}

function getSelectedFont(fontList) {
  return fontList.options[fontList.selectedIndex].value;
}

// Populates the font lists with the list of system fonts from |fonts|.
function populateLists(fonts) {
  for (var i = 0; i < genericFamilies.length; i++) {
    var list = document.getElementById(genericFamilies[i].fontList);

    // Add special "(none)" item to indicate fallback to the non-per-script
    // font setting. The Font Settings API uses the empty string to indicate
    // fallback.
    var noneItem = document.createElement('option');
    noneItem.value = '';
    noneItem.text = '(none)';
    list.add(noneItem);

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
// each font list to the current font setting, and updates the document's lang
// so that the samples are shown in the current font setting.
function updateFontListsForScript() {
  var script = getSelectedScript();

  for (var i = 0; i < genericFamilies.length; i++) {
    var list = document.getElementById(genericFamilies[i].fontList);
    var family = genericFamilies[i].name;

    var details = {};
    details.genericFamily = family;
    details.script = script;
    // For font selection it's the script code that matters, not language, so
    // just use en for lang.
    document.body.lang = 'en-' + script;

    chrome.fontSettings.getFont(details, getFontHandler(list));
  }

  if (typeof(scriptSpecificSampleText[script]) != 'undefined')
    sample = scriptSpecificSampleText[script];
  else
    sample = defaultSampleText;
  for (var i = 0; i < sampleTextDivIds.length; i++) {
    document.getElementById(sampleTextDivIds[i]).innerText = sample;
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

function clearAllSettings() {
  var scripts =
      ["Afak", "Arab", "Armi", "Armn", "Avst", "Bali", "Bamu", "Bass", "Batk",
       "Beng", "Blis", "Bopo", "Brah", "Brai", "Bugi", "Buhd", "Cakm", "Cans",
       "Cari", "Cham", "Cher", "Cirt", "Copt", "Cprt", "Cyrl", "Cyrs", "Deva",
       "Dsrt", "Dupl", "Egyd", "Egyh", "Egyp", "Elba", "Ethi", "Geor", "Geok",
       "Glag", "Goth", "Gran", "Grek", "Gujr", "Guru", "Hang", "Hani", "Hano",
       "Hans", "Hant", "Hebr", "Hluw", "Hmng", "Hung", "Inds", "Ital", "Java",
       "Jpan", "Jurc", "Kali", "Khar", "Khmr", "Khoj", "Knda", "Kpel", "Kthi",
       "Lana", "Laoo", "Latf", "Latg", "Latn", "Lepc", "Limb", "Lina", "Linb",
       "Lisu", "Loma", "Lyci", "Lydi", "Mand", "Mani", "Maya", "Mend", "Merc",
       "Mero", "Mlym", "Moon", "Mong", "Mroo", "Mtei", "Mymr", "Narb", "Nbat",
       "Nkgb", "Nkoo", "Nshu", "Ogam", "Olck", "Orkh", "Orya", "Osma", "Palm",
       "Perm", "Phag", "Phli", "Phlp", "Phlv", "Phnx", "Plrd", "Prti", "Rjng",
       "Roro", "Runr", "Samr", "Sara", "Sarb", "Saur", "Sgnw", "Shaw", "Shrd",
       "Sind", "Sinh", "Sora", "Sund", "Sylo", "Syrc", "Syre", "Syrj", "Syrn",
       "Tagb", "Takr", "Tale", "Talu", "Taml", "Tang", "Tavt", "Telu", "Teng",
       "Tfng", "Tglg", "Thaa", "Thai", "Tibt", "Tirh", "Ugar", "Vaii", "Visp",
       "Wara", "Wole", "Xpeo", "Xsux", "Yiii", "Zmth", "Zsym", "Zyyy"];
  var families =
      ["standard", "sansserif", "serif", "fixed", "cursive", "fantasy"];
  for (var i = 0; i < scripts.length; i++) {
    for (var j = 0; j < families.length; j++) {
      chrome.fontSettings.clearFont({
        script: scripts[i],
        genericFamily: families[j]
      });
    }
  }

  chrome.fontSettings.clearDefaultFixedFontSize();
  chrome.fontSettings.clearDefaultFontSize();
  chrome.fontSettings.clearMinimumFontSize();
}

function init() {
  scriptList = document.getElementById('scriptList');
  scriptList.addEventListener('change',
                              updateFontListsForScript);

  // Populate the font lists.
  chrome.fontSettings.getFontList(populateLists);

  // Add change handlers to the font lists.
  for (var i = 0; i < genericFamilies.length; i++) {
    var list = document.getElementById(genericFamilies[i].fontList);
    var handler = getFontChangeHandler(list, genericFamilies[i].name);
    list.addEventListener('change', handler);
  }

  chrome.fontSettings.onFontChanged.addListener(
      updateFontListsForScript);

  initFontSizePref(
      'defaultFontSize',
      ['standardFontSample', 'serifFontSample', 'sansSerifFontSample'],
      chrome.fontSettings.getDefaultFontSize,
      chrome.fontSettings.setDefaultFontSize,
      chrome.fontSettings.onDefaultFontSizeChanged);
  initFontSizePref(
      'defaultFixedFontSize',
      ['fixedFontSample'],
      chrome.fontSettings.getDefaultFixedFontSize,
      chrome.fontSettings.setDefaultFixedFontSize,
      chrome.fontSettings.onDefaultFixedFontSizeChanged);
  initFontSizePref(
      'minFontSize',
      ['minFontSample'],
      chrome.fontSettings.getMinimumFontSize,
      chrome.fontSettings.setMinimumFontSize,
      chrome.fontSettings.onMinimumFontSizeChanged);

  var clearButton = document.getElementById('clearButton');
  clearButton.addEventListener('click', clearAllSettings);
}

document.addEventListener('DOMContentLoaded', init);
