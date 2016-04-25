# Behavior of Download File Types in Chrome

This describes how to adjust file-type download behavior in
Chrome including interactions with Safe Browsing.  The metadata described
here, and stored in `download_file_types.asciipb`, will be both baked into
Chrome released and pushable to Chrome between releases. http://crbug.com/596555

Rendered version of this file: https://chromium.googlesource.com/chromium/src/+/master/chrome/browser/resources/safe_browsing/README.md


## Procedure for adding a new type
  * Edit `download_file_types.asciipb`. Update `histograms.xml`
  * Get it reviewed, submit.
  * Push via component update (PROCEDURE TBD)

## Guidelines for a DownloadFileType entry:
See `download_file_types.proto` for all fields.

  * `extension`: Value must be unique within the config. It should be
    lowercase ASCII and not contain a dot.  If there _is_ a duplicate,
    first one wins.  Only the `default_file_type` should leave this unset.

  * `uma_value`: Value must be unique and match one in the
    `SBClientDownloadExtensions` enum in `histograms.xml`.

  * `is_archive`: `True` if this filetype is a container for other files.
     Leave it unset for `false`.

  * `platform_settings`: (repeated) Must have one entry with an unset
     `platform` field, and optionally additional entries with overrides
     for one or more platforms.  An unset `platform` field acts as a
     default for any platforms that don't have an override.  There should
     not be two settings with the same `platform`, but if there are,
     first one wins.  Keep them sorted by platform.

  * `platform_settings.danger_level`: (required)
    * `NOT_DANGEROUS`: Safe to download and open, even if the download
       was accidental.
    * `DANGEROUS`: Always warn the user that this file may harm their
      computer.  We let them continue or discard the file.  If Safe
      Browsing returns a SAFE verdict, we still warn the user.
    * `ALLOW_ON_USER_GESTURE`: Warn the user normally but skip the warning
      if there was a user gesture or the user visited this site before
      midnight last night (i.e. is a repeat visit).  If Safe Browsing
      returns a SAFE verdict for this file, it won't show a warning.

  * `platform_settings.auto_open_hint`: Required.
    * `ALLOW_AUTO_OPEN`: File type can be opened automatically if the user
      selected that option from the download tray on a previous download
      of this type.
    * `DISALLOW_AUTO_OPEN`:  Never let the file automatically open.
      Files that should be disallowed from auto-opening include those that
      execute arbitrary or harmful code with user privileges, or change
      configuration of the system to cause harmful behavior immediately
      or at some time in the future. We *do* allow auto-open for files
      that upon opening sufficiently warn the user about the fact that it
      was downloaded from the internet and can do damage.  **Note**:
      Some file types (e.g.: .local and .manifest) aren't dangerous
      to open.  However, their presence on the file system may cause
      potentially dangerous changes in behavior for other programs. We
      allow automatically opening these file types, but always warn when
      they are downloaded.

  * `platform_settings.ping_setting`:  Required.  This controls what sort
     of ping is sent to Safe Browsing and if a verdict is checked before
     the user can access the file.
    * `SAMPLED_PING`: Don't send a full Safe Browsing ping, but
       send a no-PII "light-ping" for a random sample of SBER users.
       This should be the default for unknown types.  The verdict won't
       be used.
    * `NO_PING`:  Don’t send any pings.  This file is whitelisted. All
      NOT_DANGEROUS files should normally use this.
    * `FULL_PING`: Send full pings and use the verdict. All dangerous
      file should use this.

  * TODO(nparker): Support this: `platform_settings.unpacker`:
     optional. Specifies which archive unpacker internal to Chrome
     should be used.  If potentially dangerous file types are found,
     Chrome will send a full-ping for the entire file.  Otherwise, it'll
     follow the ping settings. Can be one of UNPACKER_ZIP or UNPACKER_DMG.

## Guidelines for the top level DownloadFileTypeConfig entry:
  * `version_id`: Must be increased (+1) every time the file is checked in.
     Will be logged to UMA.

  * `sampled_ping_probability`: For what fraction of extended-reporting
    users' downloads with unknown extensions (or
    ping_setting=SAMPLED_PING) should we send light-pings? [0.0 .. 1.0]

  * `file_type`: The big list of all known file types. Keep them
     sorted by extension.

  * `default_file_type`: Settings used if a downloaded file is not in
    the above list.  `extension` is ignored, but other settings are used.
    The ping_setting should be SAMPLED_PING for all platforms.

