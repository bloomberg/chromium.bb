[CrOS MultiDevice]

Note: This component is in the process of being built. It will eventually
      contain implementation that was previously part of
      //components/cryptauth and //components/proximity_auth.

This component contains the implementation of two APIs:
(1) DeviceSync API: This API syncs metadata about other devices tied to a given
    Google account. It contacts the CryptAuth back-end to enroll the current
    device and sync down new data about other devices.
(2) SecureChannnel API: This API gives clients the ability to establish
    connections to remote devices synced via the DeviceSync API. Once a
    communication channel is established, the API allows clients to send and
    receive messages through the channel.
