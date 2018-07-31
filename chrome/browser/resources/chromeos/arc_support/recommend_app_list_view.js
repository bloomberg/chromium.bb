// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var selectedApps = /** @dict */ {};

function generateContents(appIcon, appTitle, appPackageName) {
  var doc = document;
  var recommendAppsContainer = doc.getElementById('recommend-apps-container');
  var item = doc.createElement('div');
  item.classList.add('item');
  item.classList.add('checked');
  item.setAttribute('data-packagename', appPackageName);

  var imagePicker = doc.createElement('div');
  imagePicker.classList.add('image-picker');
  item.appendChild(imagePicker);

  var chip = doc.createElement('div');
  chip.classList.add('chip');
  chip.addEventListener('mousedown', addRippleCircle);
  chip.addEventListener('mouseup', toggleCheckStatus);
  chip.addEventListener('animationend', removeRippleCircle);
  item.appendChild(chip);

  var img = doc.createElement('img');
  img.classList.add('app-icon');
  img.setAttribute('src', decodeURIComponent(appIcon));

  var title = doc.createElement('span');
  title.classList.add('app-title');
  title.innerHTML = appTitle;

  chip.appendChild(img);
  chip.appendChild(title);

  recommendAppsContainer.appendChild(item);
}

// Add a layer on top of the chip to create the ripple effect.
function addRippleCircle(e) {
  let chip = e.currentTarget;
  let item = chip.parentNode;
  let offsetX = e.pageX - item.offsetLeft;
  let offsetY = e.pageY - item.offsetTop;
  chip.style.setProperty('--x', offsetX);
  chip.style.setProperty('--y', offsetY);
  chip.innerHTML += '<div class="ripple"></div>';
}

// After the animation ends, remove the ripple layer.
function removeRippleCircle(e) {
  let chip = e.currentTarget;
  let rippleLayers = chip.querySelectorAll('.ripple');
  for (let rippleLayer of rippleLayers) {
    if (rippleLayer.className === 'ripple') {
      rippleLayer.remove();
    }
  }
}

// Toggle the check status of an app. If an app is selected, add the "checked"
// class so that the checkmark is visible. Otherwise, remove the checked class.
function toggleCheckStatus(e) {
  var item = e.currentTarget.parentNode;
  item.classList.toggle('checked');
}

function getSelectedPackages() {
  var selectedPackages = [];
  var checkedItems = document.getElementsByClassName('checked');
  for (var checkedItem of checkedItems) {
    selectedPackages.push(checkedItem.dataset.packagename);
  }
  return selectedPackages;
}

// Add the scrolling shadow effect.
(function() {
const shadowThreshold = 5;
var doc = document;
doc.getElementById('recommend-apps-container').onscroll = function() {
  doc.getElementById('scroll-top')
      .classList[this.scrollTop > shadowThreshold ? 'add' : 'remove']('shadow');
  doc.getElementById('scroll-bottom')
      .classList
          [this.scrollHeight - this.clientHeight - this.scrollTop <
                   shadowThreshold ?
               'remove' :
               'add']('shadow');
};
})();