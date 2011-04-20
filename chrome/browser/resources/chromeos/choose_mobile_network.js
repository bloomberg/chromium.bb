// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('mobile', function() {

  function ChooseNetwork() {
  }

  cr.addSingletonGetter(ChooseNetwork);

  ChooseNetwork.prototype = {
    networks_: [],
    showNetworks_: function(networks) {
      this.networks_ = networks;

      if (networks.length == 0) {
        $('scanning').hidden = true;
        $('no-mobile-networks').hidden = false;
        return;
      }

      var container = $('choosing');
      container.innerHTML = '';
      for (var i in networks) {
        var elem = document.createElement('div');
        elem.innerHTML =
            '<input type="radio" name="network" id="network' + i + '" />' +
            '<label for="network' + i + '" id="label' + i + '"></label>';
        container.appendChild(elem);
        $('label' + i).textContent = networks[i].operatorName;
        if (networks[i].status == 'current') {
          $('network' + i).checked = true;
          $('connect').disabled = false;
        } else if (networks[i].status == 'forbidden') {
          $('network' + i).disabled = true;
          elem.className = 'disabled';
        } else {
          $('network' + i).addEventListener('click', function(event) {
            $('connect').disabled = false;
          });
        }
      }
      $('scanning').hidden = true;
      $('choosing').hidden = false;
    },
    connect_: function() {
      for (var i in this.networks_) {
        if ($('network' + i).checked) {
          chrome.send('connect', [ this.networks_[i].networkId ]);
          ChooseNetwork.close();
          return;
        }
      }
    }
  };

  ChooseNetwork.cancel = function() {
    chrome.send('cancel');
    ChooseNetwork.close();
  };

  ChooseNetwork.close = function() {
    window.close();
  };

  ChooseNetwork.connect = function() {
    ChooseNetwork.getInstance().connect_();
  };

  ChooseNetwork.initialize = function() {
    $('cancel').addEventListener('click', function(event) {
      ChooseNetwork.cancel();
    });

    $('connect').disabled = true;
    $('connect').addEventListener('click', function(event) {
      ChooseNetwork.connect();
    });
  };

  ChooseNetwork.showNetworks = function(networks) {
    ChooseNetwork.getInstance().showNetworks_(networks);
  };

  // Export
  return {
    ChooseNetwork: ChooseNetwork
  };
});
