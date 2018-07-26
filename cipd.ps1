# Copyright (c) 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

$myPath = Split-Path $MyInvocation.MyCommand.Path -Parent

function GetEnvVar([string] $key, [scriptblock] $defaultFn) {
    if (Test-Path "Env:\$key") {
        return (Get-ChildItem "Env:\$key").Value
    }
    return $defaultFn.Invoke()
}

$cipdClientVer = GetEnvVar "CIPD_CLIENT_VER" {
  Get-Content (Join-Path $myPath -ChildPath 'cipd_client_version') -TotalCount 1
}
$cipdClientSrv = GetEnvVar "CIPD_CLIENT_SRV" {
  "https://chrome-infra-packages.appspot.com"
}

$plat="windows"
if ([environment]::Is64BitOperatingSystem)  {
  $arch="amd64"
} else {
  $arch="386"
}

$url = "$cipdClientSrv/client?platform=$plat-$arch&version=$cipdClientVer"
$client = Join-Path $myPath -ChildPath ".cipd_client.exe"

try {
  $depot_tools_version = &git -C $myPath rev-parse HEAD 2>&1
  if ($LastExitCode -eq 0) {
    $user_agent = "depot_tools/$depot_tools_version"
  } else {
    $user_agent = "depot_tools/???"
  }
} catch [System.Management.Automation.CommandNotFoundException] {
  $user_agent = "depot_tools/no_git/???"
}

$Env:CIPD_HTTP_USER_AGENT_PREFIX = $user_agent

# Use a lock fle to prevent simultaneous processes from stepping on each other.
$cipd_lock = Join-Path $myPath -ChildPath '.cipd_client.lock'
while ($true) {
  $cipd_lock_file = $false
  try {
      $cipd_lock_file = [System.IO.File]::OpenWrite($cipd_lock)

      echo "Bootstrapping cipd client for $plat-$arch from $url..."
      $wc = (New-Object System.Net.WebClient)
      $wc.Headers.add('User-Agent', $user_agent)
      $wc.DownloadFile($url, $client)
      break

  } catch [System.IO.IOException] {
      echo "CIPD bootstrap lock is held, trying again after delay..."
      Start-Sleep -s 1
  } catch {
      $err = $_.Exception.Message
      echo "CIPD bootstrap failed: $err"
      throw $err
  } finally {
      if ($cipd_lock_file) {
          $cipd_lock_file.Close()
          $cipd_lock_file = $false
          try {
            [System.IO.File]::Delete($cipd_lock)
          } catch {}
      }
  }
}
