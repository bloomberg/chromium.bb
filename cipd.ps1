# Copyright (c) 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

$myPath = Split-Path $MyInvocation.MyCommand.Path -Parent

function GetEnvVar([string] $key, [scriptblock] $defaultFn) {
    if (Test-Path "Env:\$key") {
        return Get-ChildItem $Env $key
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
if (!(Test-Path $client)) {
    echo "Bootstrapping cipd client for $plat-$arch..."
    echo "From $url"
    # TODO(iannucci): It would be really nice if there was a way to get this to
    # show progress without also completely destroying the download speed, but
    # I can't seem to find a way to do it. Patches welcome :)
    $wc = (New-Object System.Net.WebClient)
    $wc.Headers.add('User-Agent', $user_agent)
    $wc.DownloadFile($url, $client)
}

$_ = & $client selfupdate -version "$cipdClientVer"
if ($LastExitCode -ne 0) {
    Write-Host "selfupdate failed: " -ForegroundColor Red -NoNewline
    Write-Host "run ``set CIPD_HTTP_USER_AGENT_PREFIX=$user_agent/manual && $client selfupdate -version $cipdClientVer`` to diagnose`n" -ForegroundColor White
}

& $client @args
exit $LastExitCode
