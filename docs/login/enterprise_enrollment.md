# Enterprise Enrollment on Login

The easiest way to test enterprise enrollment on login is to use an actual
enterprise account. If you don't have one, reach out a teammate; anyone with an
account can add new accounts.

Once you have an enterprise account, run chrome and [enroll the device](https://support.google.com/chrome/a/answer/1360534?hl=en). The shortcut combo is
`Ctrl+Alt+E`.

Note, that you can only enroll device if it does not have owner (no user have
signed in on the device, nor it was already enrolled). If device have an owner
you would need to clear the ownership first. If you're testing on device and
wish to clear enrollment state, the easiest way is to run
`crossystem clear_tpm_owner_request=1` and then reboot. This clears
TPM state which will destroy cryptohome and enrollment state. When the device
boots next it will check and see if it needs to be force re-enrolled.

Policy can be configured at admin.google.com; log in with your enterprise
account. Whoever created the account should have granted you superuser
privileges. You may need to log in using an incognito window if your primary
Google account is part of an enterprise domain.

Few notable policy sections in admin.google.com under
`Device Management>Chrome>Device Settings` are `Enrollment & Access` that
controls if device would be automatically re-enrolled after wipe and
`Kiosk settings` that allows to configure public sessions / Kiosk mode for
the ChromeOS device.

When you're changing policies in admin.google.com, pay attention to the
organization you are modifying. Try to only adjust your test organization to
avoid propagating changes to other users.