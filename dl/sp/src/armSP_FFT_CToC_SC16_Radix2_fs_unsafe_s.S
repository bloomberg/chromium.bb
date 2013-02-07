;//
;// 
;// File Name:  armSP_FFT_CToC_SC16_Radix2_fs_unsafe_s.s
;// OpenMAX DL: v1.0.2
;// Last Modified Revision:   6693
;// Last Modified Date:       Tue, 10 Jul 2007
;// 
;// (c) Copyright 2007-2008 ARM Limited. All Rights Reserved.
;// 
;// 
;//
;// Description:
;// Compute a Radix 2 FFT stage for a N point complex signal
;// 
;// 

        
;// Include standard headers

        INCLUDE omxtypes_s.h
        INCLUDE armCOMM_s.h
        
        M_VARIANTS CortexA8
        
;// Import symbols required from other files
;// (For example tables)
    
        
        
        
;// Set debugging level        
;//DEBUG_ON    SETL {TRUE}



;// Guarding implementation by the processor name
    
    
            
;// Guarding implementation by the processor name
    
    IF  CortexA8 
    
;//Input Registers

pSrc            RN  0
pDst            RN  2
pTwiddle        RN  1
pPingPongBuf    RN  5
subFFTNum       RN  6
subFFTSize      RN  7


;//Output Registers


;//Local Scratch Registers

pointStep        RN  3
outPointStep     RN  3
grpSize          RN  4
setCount         RN  4
step             RN  8
dstStep          RN  8

;// Neon Registers

dX0             DN  D0.S16
dX1             DN  D1.S16
dY0             DN  D2.S16
dY1             DN  D3.S16
dX0S32          DN  D0.S32
dX1S32          DN  D1.S32
dY0S32          DN  D2.S32
dY1S32          DN  D3.S32


        MACRO
        FFTSTAGE $scaled, $inverse, $name
        
        ;// Define stack arguments
        
        
        ;// update subFFTSize and subFFTNum into RN6 and RN7 for the next stage
        
        
        MOV        subFFTSize,#2
        LSR        grpSize,subFFTNum,#1  
        MOV        subFFTNum,grpSize 
        
        
        ;// pT0+1 increments pT0 by 8 bytes
        ;// pT0+pointStep = increment of 4*pointStep bytes = 2*grpSize bytes
        ;// Note: outPointStep = pointStep for firststage
        ;// Note: setCount = grpSize/2 (reuse the updated grpSize for setCount)
        
        MOV        pointStep,grpSize,LSL #2
        RSB        step,pointStep,#4 
        
        
        ;// Loop on the sets for grp zero: 1 set at a time

grpZeroSetLoop$name        
        
        VLD1    {dX0S32[0]},[pSrc],pointStep
        VLD1    {dX1S32[0]},[pSrc],step                   ;// step = -pointStep + 4
        SUBS    setCount,setCount,#1              ;// decrement the loop counter
        
        IF $scaled
        
            VHADD    dY0,dX0,dX1
            VHSUB    dY1,dX0,dX1
        
        ELSE
        
            VADD    dY0,dX0,dX1
            VSUB    dY1,dX0,dX1
        
         
        ENDIF
        
        VST1    {dY0S32[0]},[pDst],outPointStep
        VST1    {dY1S32[0]},[pDst],dstStep                  ;// dstStep =  step = -pointStep + 4
               
        BGT     grpZeroSetLoop$name
        
        
        ;// reset pSrc to pDst for the next stage
        SUB     pSrc,pDst,pointStep                     ;// pDst -= 2*grpSize 
        MOV     pDst,pPingPongBuf
                
        MEND
        
        
                
        M_START armSP_FFTFwd_CToC_SC16_Radix2_fs_OutOfPlace_unsafe,r4
        FFTSTAGE {FALSE},{FALSE},FWD
        M_END

        
        
        M_START armSP_FFTInv_CToC_SC16_Radix2_fs_OutOfPlace_unsafe,r4
        FFTSTAGE {FALSE},{TRUE},INV
        M_END
 
        
        
        M_START armSP_FFTFwd_CToC_SC16_Sfs_Radix2_fs_OutOfPlace_unsafe,r4
        FFTSTAGE {TRUE},{FALSE},FWDSFS
        M_END

        
        
        M_START armSP_FFTInv_CToC_SC16_Sfs_Radix2_fs_OutOfPlace_unsafe,r4
        FFTSTAGE {TRUE},{TRUE},INVSFS
        M_END

        
    ENDIF                                                           ;//CORTEXA8
    
     
    END