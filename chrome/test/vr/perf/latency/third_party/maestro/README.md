maestro.py
==========

This Python class supports Pololu's Maestro servo controller over USB serial. Great for use with the Raspberry Pi.

The class includes methods to control servos (position, speed, acceleration), read servo position, and start/stop Maestro scripts.  See Pololu's on-line documentation to learn about the full capabilities of this nifty micro-controller.

Pololu's Maestro Windows installer sets up the Maestro Control Center, used to configure, test and program the controller.  Be sure the Maestro is configured for "USB Dual Port" serial mode, which is [not the default](https://www.pololu.com/docs/0J40/3.c).

You'll need to have the 'pyserial' Python module installed to use maestro.py.

For Linux, download pyserial-2.7.tar.gz from http://sourceforge.net/projects/pyserial/files/pyserial/

    wget http://sourceforge.net/projects/pyserial/files/pyserial/2.7/pyserial-2.7.tar.gz

 and then install

    tar –zxf pyserial-2.7.tar.gz
    cd pyserial-2.7
    sudo python setup.py install

Alternatively, if you have pip available, pyserial can be installed as follows:

    python -m pip install pyserial

Check out http://pyserial.readthedocs.io/en/latest/pyserial.html#installation for other install options.

Example usage of maestro.py:

    import maestro.py
    servo = maestro.Controller()
    servo.setAccel(0,4)      #set servo 0 acceleration to 4
    servo.setTarget(0,6000)  #set servo to move to center position
    servo.close

For use on Windows, you'll need to provide the COM port assigned to the Maestro Command Port.  You can indentify the port by starting Device Manager and looking under Ports (COM & LPT).  Here's how to instantiate the controller for Windows.

    import maestro.py
    m = maestro.Controller('COM3')
    
