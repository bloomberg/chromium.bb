// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer('log-panel', {
  publish: {
    /**
     * List of displayed logs.
     * @type {Array.<{{
     *    text: string,
     *    date: string,
     *    source: string
     * }}>}
     */
    logs: null,
  },

  /**
   * Called when an instance is created.
   */
  created: function() {
    this.logs = [
      { text: 'sh: missing ]' },
      { text: 'Starting Mount filesystems on boot [ OK ]' },
      { text: 'Starting Populate /dev filesystem [ OK ]' },
      { text: 'Stopping Populate /dev filesystem [ OK ]' },
      { text: 'Starting Create /var/run/ccache [ OK ]' },
      { text: 'Starting Populate and link to /run filesystem [ OK ]' },
      { text: 'Starting Create /run/credentials-cache [ OK ]' },
      { text: 'Stopping Create /var/run/ccache [ OK ]' },
      { text: 'Stopping Populate and link to /run filesystem [ OK ]' },
      { text: 'Stopping Create /run/credentials-cache [ OK ]' },
      { text: 'Starting Signal sysvinit that the rootfs is mounted [ OK ]' },
      { text: 'Starting Clean /tmp directory [ OK ]' },
      { text: 'Stopping Track if upstart is running in a container [ OK ]' },
      { text: 'Starting mount available cgroup filesystems [ OK ]' },
      { text: 'Stopping Clean /tmp directory [ OK ]' },
      { text: 'Starting Initialize or finalize resolvconf [ OK ]' },
      { text: 'mount: none already mounted or /dev/cgroup/cpu busy' },
      { text: 'Starting Set custom IMA policy [ OK ]' },
      { text: 'Stopping Set custom IMA policy [ OK ]' },
      { text: 'Starting Bridge udev events into upstart [ OK ]' },
      { text: 'Starting detecthammer security monitoring daemon [ OK ]' },
      { text: 'Starting linhelm security monitoring daemon [ OK ]' },
      { text: 'Starting Fetch otp over bluetooth on demand [ OK ]' },
      { text: 'Starting device node and kernel event manager [ OK ]' },
      { text: 'Starting Enabling additional executable binary formats [ OK ]' },
      { text: 'Starting flush early job output to logs [ OK ]' },
      { text: 'Stopping Mount filesystems on boot [ OK ]' },
      { text: 'Starting NFSv4 id <-> name mapper [ OK ]' },
      { text: 'Stopping flush early job output to logs [ OK ]' },
      { text: 'Starting load modules from /etc/modules [ OK ]' },
      { text: 'Starting load modules from /etc/modules [ OK ]' },
      { text: 'Starting log initial device creation [ OK ]' },
      { text: 'Starting D-Bus system message bus [ OK ]' },
      { text: 'Stopping load modules from /etc/modules [ OK ]' },
      { text: 'Stopping load modules from /etc/modules [ OK ]' },
      { text: 'Starting Uncomplicated firewall [ OK ]' },
      { text: 'Starting Bridge file events into upstart [ OK ]' },
      { text: 'Starting bluetooth daemon [ OK ]' },
      { text: 'Starting SystemD login management service [ OK ]' },
      { text: 'Starting system logging daemon [ OK ]' },
      { text: 'Starting configure network device security [ OK ]' },
      { text: 'Starting configure network device security [ OK ]' },
      { text: 'Stopping cold plug devices [ OK ]' },
      { text: 'Starting configure network device security [ OK ]' },
      { text: 'Starting CUPS printing spooler/server [ OK ]' },
      { text: 'Starting configure network device [ OK ]' },
      { text: 'Starting Mount network filesystems [ OK ]' },
      { text: 'Starting Upstart job to start rpcbind on boot only [ OK ]' },
      { text: 'Starting Failsafe Boot Delay [ OK ]' },
      { text: 'Stopping log initial device creation [ OK ]' },
      { text: 'Stopping Upstart job to start rpcbind on boot only [ OK ]' },
      { text: 'Starting load fallback graphics devices [ OK ]' },
      { text: 'Starting configure network device security [ OK ]' },
      { text: 'Starting configure network device [ OK ]' },
      { text: 'Stopping load fallback graphics devices [ OK ]' },
      { text: 'Stopping Failsafe Boot Delay [ OK ]' },
      { text: 'Stopping Mount network filesystems [ OK ]' },
      { text: 'Starting System V initialisation compatibility [ OK ]' },
      { text: 'Starting configure virtual network devices [ OK ]' },
      { text: 'Initializing random number generator...         [ OK ]' },
      { text: 'Setting up X socket directories...         [ OK ]' },
      { text: 'Starting configure network device [ OK ]' },
      { text: 'Starting modem connection manager [ OK ]' },
      { text: 'Starting configure network device security [ OK ]' },
      { text: 'Starting RPC portmapper replacement [ OK ]' },
      { text: 'Stopping System V initialisation compatibility [ OK ]' },
      { text: 'Starting Create /var/run/ccache [ OK ]' },
      { text: 'Stopping Create /var/run/ccache [ OK ]' },
      { text: 'Starting Monitor for unauthorized access to cookies. [ OK ]' },
      { text: 'Starting System V runlevel compatibility [ OK ]' },
      { text: 'Starting anac(h)ronistic cron [ OK ]' },
      { text: 'Starting ACPI daemon [ OK ]' },
      { text: 'Starting regular background program processing daemon [ OK ]' },
      { text: 'Starting deferred execution scheduler [ OK ]' },
      { text: 'Starting Run the nvidia-updater Upstart task before the ' +
              'goobuntu-updater Upstart task [ OK ]' },
      { text: 'Starting build kernel modules for current kernel [ OK ]' },
      { text: 'Starting CPU interrupts balancing daemon [ OK ]' },
      { text: 'Starting rpcsec_gss daemon [ OK ]' },
      { text: 'Starting NSM status monitor [ OK ]' },
      { text: 'Stopping anac(h)ronistic cron [ OK ]' },
      { text: 'Starting Run the nvidia-updater during startup [ OK ]' },
      { text: 'Starting regular background program processing daemon [ OK ]' },
      { text: 'Starting KVM [ OK ]' },
      { text: 'Starting automatic crash report generation [ OK ]' },
      { text: 'Starting Automounter [ OK ]' },
      { text: 'Stopping Restore Sound Card State [ OK ]' },
      { text: 'Starting libvirt daemon [ OK ]' },
      { text: 'gdm start/starting' },
      { text: 'initctl: Unknown job: kdm' },
      { text: 'lightdm start/starting' },
      { text: 'initctl: Unknown job: lxdm' },
      { text: 'Starting Bridge socket events into upstart [ OK ]' },
      { text: 'Skipping profile in /etc/apparmor.d/disable: usr.bin.firefox' },
      { text: 'Starting AppArmor profiles         [ OK ]' },
      { text: 'Checking for available NVIDIA updates ...' },
      { text: 'Starting audit daemon auditd         [ OK ]' },
      { text: 'Starting Dropbear SSH server: [abort] NO_START is not ' +
              'set to zero in /etc/default/dropbear' },
      { text: 'Starting logs-exporterd         [ OK ]' },
      { text: 'Starting Machine Check Exceptions decoder: mcelog.' },
      { text: 'ERROR: [Errno -2] Name or service not known' },
      { text: '2015-04-01 13:12:35,580:INFO:Daemon process started in the ' +
              'background, logging to ' +
              '"/tmp/chrome_remote_desktop_20150401_131235_3BiEyp"' },
      { text: 'Using host_id: cd1af87e-488c-38ec-6b84-40e028bbf174' },
      { text: 'Launching X server and X session.' },
      { text: 'Starting NTP server ntpd         [ OK ]' },
      { text: 'Not starting internet superserver: no services enabled' },
      { text: 'Starting OpenCryptoki PKCS#11 slot daemon: pkcsslotd.' },
      { text: 'Starting OSS Proxy Daemon osspd         [ OK ]' },
      { text: 'Starting /usr/bin/Xvfb-randr on display :20' },
      { text: 'Starting network connection manager [ OK ]' },
      { text: 'INFO: Configuring network-manager from upstart' },
      { text: '/proc/self/fd/9: line 6: INFO: Configuring network-manager ' +
              'from upstart: command not found' },
      { text: 'Updating connection SNAX' },
      { text: 'Xvfb is active.' },
      { text: 'Completed successfully' },
      { text: 'INFO: Success' },
      { text: '/proc/self/fd/9: line 6: INFO: Success: command not found' },
      { text: 'Starting Postfix Mail Transport Agent postfix         [ OK ]' },
      { text: 'Stopping build kernel modules for current kernel [ OK ]' },
      { text: 'Launching host process' },
      { text: 'Stopping Run the nvidia-updater during startup [ OK ]' },
      { text: 'Stopping Run the nvidia-updater Upstart task before the ' +
              'goobuntu-updater Upstart task [ OK ]' },
      { text: 'Waiting up to 60s for the hostname ...' },
      { text: 'Recovering schroot sessions         [ OK ]' },
      { text: '[33m*39;49m Not starting S.M.A.R.T. daemon smartd, disabled ' +
              'via /etc/default/smartmontools' },
      { text: 'Starting OpenBSD Secure Shell server...         [ OK ]' },
      { text: 'No response from daemon. It may have crashed, or may still ' +
              'be running in the background.' },
      { text: 'Host ready to receive connections.' },
      { text: 'Log file: /tmp/chrome_remote_desktop_20150401_131235_3BiEyp' },
      { text: 'Starting VirtualBox kernel modules         [ OK ]' },
      { text: 'saned disabled; edit /etc/default/saned' },
      { text: 'Restoring resolver state...         [ OK ]' },
      { text: 'The hostname has not changed' },
      { text:
        'access-control-allow-origin:*\n        ' +
        'cache-control:public, max-age=600\n        ' +
        'date:Thu, 16 Apr 2015 21:15:23 GMT\n        ' +
        'etag:"nhb8IQ"\n        ' +
        'expires:Thu, 16 Apr 2015 21:25:23 GMT\n        ' +
        'server:Google Frontend\n        ' +
        'status:304\n        ' +
        'x-google-appengine-appid:s~polymer-project\n        ' +
        'x-google-appengine-module:default\n        ' +
        'x-google-appengine-version:2015-04-13\n        ' +
        'x-google-backends:/gns/project/apphosting/appserver/prod-appengine' +
        '/ic/prod-appengine.remote-ib.appserver/380,icbie6.prod.google.com:' +
        '4489,/bns/ic/borg/ic/bns/apphosting/prod-appengine.edge.frontend/2' +
        '30,ibk19:6599,/bns/ib/borg/ib/bns/gfe-prod/shared-gfe_31_silos/1.g' +
        'fe,icna18:9845\n        ' +
        'x-google-dos-service-trace:main:apphosting,dasher:' +
        'apphosting\n        ' +
        'x-google-gfe-request-trace:icna18:9845,ibk19:6599,/bns/ic/borg/ic/' +
        'bns/apphosting/prod-appengine.edge.frontend/230,ibk19:6599,icna18:' +
        '9845\n        ' +
        'x-google-gfe-response-code-details-trace:response_code_set_by_back' +
        'end,response_code_set_by_backend\n        ' +
        'x-google-gfe-service-trace:apphosting,dasher_zoo_responder,apphost' +
        'ing\n        ' +
        'x-google-gslb-service:apphosting\n        ' +
        'x-google-netmon-label:/bns/ic/borg/ic/bns/apphosting/prod-appengin' +
        'e.edge.frontend/230\n        ' +
        'x-google-service:apphosting,apphosting\n' },
      { text: 'goobuntu-updater configuration: ARGS="--init ' +
              '--only-if-forced --verbose" BLOCK_BOOT=false' },
      { text: 'Running goobuntu-updater ...' },
      { text: 'Starting GNOME Display Manager [ OK ]' },
      { text: 'No apache MPM package installed' },
      { text: 'Starting Unblock the restart of the gdm display ' +
              'manager when it stops by emitting the ' +
              'nvidia-updater-unblock-gdm-start event [ OK ]' },
      { text: 'Stopping GNOME Display Manager [ OK ]' },
    ];
  },

  /**
   * Stores the logs that are cleared.
   * @type {Array.<Log>}
   * @private
   */
  prevLogs_: null,

  /**
   * Called after the Polymer element is initialized.
   */
  ready: function() {
    this.async(this.scrollToBottom_);
  },

  /**
   * Clears the logs.
   * @private
   */
  clearLogs: function() {
    var prevLogs = this.logs;
    this.logs = this.prevLogs;
    this.prevLogs = prevLogs;
    this.async(this.scrollToBottom_);
  },

  /**
   * Scrolls the logs container to the bottom.
   * @private
   */
  scrollToBottom_: function() {
    this.$.list.scrollTop = this.$.list.scrollHeight;
  },
});
